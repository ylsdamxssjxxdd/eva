#if defined(_MSC_VER)
#pragma warning(disable : 4244 4267)  // possible loss of data
#endif
#include "xbot.h"

xBot::xBot() {
    llama_log_set(xBot::bot_log_callback, this);  //设置回调,获取llama的日志
    QObject::connect(this, &xBot::bot_llama_log, this, &xBot::recv_llama_log);

    //初始的模型参数
    common_params_.n_gpu_layers = DEFAULT_NGL;     // gpu负载层数
    common_params_.model = "";                     //模型路径
    common_params_.cpuparams.n_threads = DEFAULT_NTHREAD;  //文字生成线程数，默认使用一半的线程数
    common_params_.cpuparams_batch.n_threads = DEFAULT_NTHREAD;  //上文处理线程数，为了简单，与文字生成线程数保持一致
    common_params_.n_ctx = DEFAULT_NCTX;           //上下文最大长度
    common_params_.n_batch = DEFAULT_BATCH;        //一次最大处理批量,主要分批次推理用户的输入,新增似乎和推理时内存泄露有关

    //初始的采样参数
    common_params_.sparams.top_p = 0.95;
    common_params_.sparams.temp = DEFAULT_TEMP;              //温度
    common_params_.sparams.penalty_repeat = DEFAULT_REPEAT;  //重复惩罚 1.0 = disabled
    common_params_.sparams.penalty_freq = 0.00;              //频率惩罚 0.0 = disabled openai
    common_params_.sparams.penalty_present = 0.00;           //同类惩罚 0.0 = disabled openai
    common_params_.flash_attn = true;  // 默认开启flash_attn

    qDebug() << "bot init over";
}

xBot::~xBot() { ; }

//模型预测推理过程
void xBot::predict(INPUTS inputs) {

    //--------------------预处理用户输入---------------------
    if (inputs.role == ROLE_TEST) {
        is_test = true;
    } else {
        is_test = false;
    }

    const size_t original_size = embd_inp.size();  //输入原长度
    // qDebug()<<"原embd_inp.size() "<<embd_inp.size();//这里就是约定的tokens

    //------------为用户的输入添加前缀和后缀,构造最终输入向量---------------
    std::vector<llama_token> line_pfx;  //前缀 向量
    std::vector<llama_token> line_inp;  //用户输入 向量
    std::vector<llama_token> line_sfx;  //后缀 向量

    //---插入前缀---
    if ((!is_complete && !is_antiprompt) && (inputs.role == ROLE_USER || inputs.role == ROLE_THOUGHT || inputs.role == ROLE_TEST))  //前缀,如果 检测出用户昵称/补完模式 则不加前缀
    {
        line_pfx = ::common_tokenize(ctx, bot_chat.input_prefix.toStdString(), false, true);
        embd_inp.insert(embd_inp.end(), line_pfx.begin(), line_pfx.end());
    }

    //---插入输入---
    line_inp = ::common_tokenize(ctx, inputs.input.toStdString(), false, true);  //用户输入,最后一个true表示会将特殊token整个分词
    embd_inp.insert(embd_inp.end(), line_inp.begin(), line_inp.end());

    //---插入后缀---
    if (!is_complete && (inputs.role == ROLE_USER))  // 补完模式则不加后缀
    {
        line_sfx = ::common_tokenize(ctx, bot_chat.input_suffix.toStdString(), false, true);
        embd_inp.insert(embd_inp.end(), line_sfx.begin(), line_sfx.end());
    } else if (inputs.role == ROLE_THOUGHT) {
        line_sfx = ::common_tokenize(ctx, bot_chat.input_suffix.toStdString() + DEFAULT_THOUGHT, false, true);
        embd_inp.insert(embd_inp.end(), line_sfx.begin(), line_sfx.end());
    } else if (inputs.role == ROLE_TEST) {
        line_sfx = ::common_tokenize(ctx, bot_chat.input_suffix.toStdString() + jtr("answer").toStdString() + ": ", false, true);
        embd_inp.insert(embd_inp.end(), line_sfx.begin(), line_sfx.end());
    }

    is_antiprompt = false;  //重置反提示标签

    push_out(inputs, line_pfx, 0);  //在输出区贴上用户昵称
    push_out(inputs, line_inp, 1);  //在输出区贴上输入内容
    push_out(inputs, line_sfx, 2);  //在输出区贴上模型昵称

    //---------------------embd_inp插入到embd中----------------------
    // qDebug()<<"插入前embd"<<view_embd(ctx,embd);
    while ((int)embd_inp.size() > n_consumed) {
        embd.push_back(embd_inp[n_consumed]);
        // common_sampler_accept(smpl, embd_inp[n_consumed], /* accept_grammar= */ false); //记录token的id
        ++n_consumed;
    }
    // qDebug()<<"插入后embd"<<view_embd(ctx,embd);
    // qDebug()<<"历史token"<<view_embd(ctx,*history_tokens);
    // qDebug()<<"embd_inp插入到embd中 "<<"n_consumed "<<n_consumed<<" embd_inp.size() "<<embd_inp.size()<<" embd.size() "<<embd.size();

    // 用户刚点击发送按钮进入debuging状态时（inputs.role 是ROLE_DEBUG 意味着刚点击）
    if (is_debuging) {
        if (!is_test) {
            if ((inputs.role != ROLE_DEBUG)) {
                bot2ui_state("DEBUGING 0 ", DEBUGING_SIGNAL);
                remain_n_remain = common_params_.n_predict;  //用来记录一次debuging过程的n_remain值
                current_output = "";                      //清空上一轮的输出记录
            }
        }
    }
    
    //-------------------------------------------------------------
    //---------------------------流式输出---------------------------
    //-------------------------------------------------------------
    is_batch = false;
    batch_time = 0.000001;
    batch_count = 0;                   //被批解码的token数
    singl_count = 0;                   //被单解码的token数
    n_remain = common_params_.n_predict;  //-1的话可以无限输出
    if (is_debuging) {
        debuging_one = 1;
        n_remain = remain_n_remain;
    }  // debuging时控制循环只进行一次, n_remain使用上一次的
    if (is_test) {
        n_remain = 1;
    }  //测试时最大输出长度强制为1
    //以下判断未启用,因为多次批解码有问题,若要启用,在ui接收到模型发送的n_ctx_train参数后,选择要拓展的倍数
    if (common_params_.n_ctx > n_ctx_train) {
        ga_n = common_params_.n_ctx / n_ctx_train + 1;
        ga_w = 512 * ga_n;
        emit bot2ui_state("bot:" + jtr("extend ctx length") + QString::number(n_ctx_train) + "->" + QString::number(common_params_.n_ctx));
    } else {
        ga_n = 1;
        ga_w = 512;
    }

    int o1 = stream();
    while (o1 == 1)  //如果解码失败返回的结果是1,则n_past+1(相当于一个空的token)并重新解码,直到解码能够成功
    {
        n_past++;  //置入一个空的记忆来缓解
        Brain_vector.push_back({n_past, -1, ""});
        batch_count--;  //空的不算数
        emit bot2ui_kv(float(n_past) / float(common_params_.n_ctx) * 100, n_past);
        if (is_debuging) {
            emit bot2expend_brainvector(Brain_vector, common_params_.n_ctx, 1);
        }  // 1强制刷新记忆矩阵
        else {
            emit bot2expend_brainvector(Brain_vector, common_params_.n_ctx);
        }
        o1 = stream();
        fail++;
        // qDebug()<<"fail times"<<fail<<"return "<<o1<<"n_past"<<n_past;
    }

    // debuging状态输出额外的信息
    if (is_debuging) {
        //打印当前缓存的上下文
        // emit bot2ui_state("bot:" + jtr("kv cache") + " " + QString::number(llama_get_kv_cache_token_count(ctx)) + " token");
    }

    // qDebug()<<"-------------------------------------------------";
    // for(int i=0;i<Brain_vector.size();++i)
    // {
    //     qDebug()<<Brain_vector.at(i).id<<Brain_vector.at(i).token<<Brain_vector.at(i).word;
    // }

    if (!is_debuging || o1 == -1)  // debuging状态就是不让bot发送pushover信号，如果是遇到停止标志或达到最大输出长度则可以
    {
        emit bot2ui_pushover();                                           //推理完成的信号
        emit bot2expend_brainvector(Brain_vector, common_params_.n_ctx, 1);  // 1强制刷新记忆矩阵
    }
}

//流式输出，0表示正常退出，-1表示遇到停止标志，1表示解码失败
int xBot::stream() 
{
    is_stop = false;
    single_timer.restart();  //后面减去batch_timer记录的时间就是单解码用时
    if (!is_debuging) {current_output = "";}
    //退出循环的情况:n_remain!=0/停止标签/推理失败/结束标志/用户昵称/额外停止标志
    while (n_remain != 0) {

        QCoreApplication::processEvents();// 接收主线事件，主要是停止信号

        // debuging时控制循环只进行一次
        if (is_debuging) {
            if (debuging_one == 0) {
                return 0;
            }
            debuging_one--;
            remain_n_remain--;  //用于下次debuging判断是否达到最大输出长度
        }

        //模型停止
        if (is_stop) {
            pick_half_utf8.clear();
            QString fianl_state;
            fianl_state = "bot:" + jtr("predict") + jtr("stop") + " ";
            if (!is_debuging) {
                fianl_state += jtr("single decode") + QString(":") + QString::number(singl_count / (single_timer.nsecsElapsed() / 1000000000.0 - batch_time), 'f', 2) + " token/s" + " " + jtr("batch decode") + QString(":") + QString::number(batch_count / batch_time, 'f', 2) + " token/s";
            }
            emit bot2ui_state(fianl_state, SUCCESS_SIGNAL);
            emit bot2ui_stopover();  //完成停止的信号
            is_stop = false;
            return 0;
        }

        //------------------------------解码-----------------------------------
        if (llama_model_has_encoder(model)) 
        {
            int enc_input_size = embd_inp.size();
            llama_token *enc_input_buf = embd_inp.data();

            if (llama_encode(ctx, llama_batch_get_one(enc_input_buf, enc_input_size))) {
                emit bot2ui_state("bot: failed to eval in encoder", EVA_SIGNAL);
                return 0;
            }

            llama_token decoder_start_token_id = llama_model_decoder_start_token(model);
            if (decoder_start_token_id == -1) {
                decoder_start_token_id = llama_token_bos(model);
            }

            embd_inp.clear();
            embd_inp.push_back(decoder_start_token_id);
        }
        
        if (!embd.empty()) {
            //输入的上下文长度超过阈值直接截断
            int max_embd_size = common_params_.n_ctx - 4 - system_tokens.size();
            if ((int)embd.size() > max_embd_size) {
                const int skipped_tokens = (int)embd.size() - max_embd_size;
                embd.resize(max_embd_size);
                emit bot2ui_state("bot:" + jtr("The length of the input context exceeds") + QString::number(max_embd_size) + " " + jtr("skip") + QString::number(skipped_tokens) + " token", WRONG_SIGNAL);
            }
            //上下文缓存超过n_ctx截断处理一半上下文, 但是保留系统指令
            if (ga_n == 1) {
                while (n_past + (int)embd.size() >= common_params_.n_ctx - 1) {
                    const int n_keep = system_tokens.size();  //需要保留的token长度
                    const int n_left = n_past - n_keep;       //除了需要保留的剩下的token长度
                    const int n_discard = n_left / 2;         //待删除的token长度
                    // qDebug()<<"n_past"<<n_past<<"n_keep"<<n_keep<<"n_left"<<n_left<<"n_discard"<<n_discard;
                    //导致批解码失败的元首
                    llama_kv_cache_seq_rm(ctx, 0, n_keep, n_keep + n_discard);               //删除中间一段缓存(n_keep -> n_keep + n_discard)
                    llama_kv_cache_seq_add(ctx, 0, n_keep + n_discard, n_past, -n_discard);  //把这一段缓存(n_keep + n_discard -> n_past)向后移构成新的缓存

                    //重构记忆向量
                    std::vector<Brain_Cell> temp_vector = Brain_vector;
                    Brain_vector.clear();
                    for (int i = 0; i < system_tokens.size(); ++i)  //系统指令是保留的
                    {
                        Brain_vector.push_back({i + 1, system_tokens.at(i), QString::fromStdString(common_token_to_piece(ctx, system_tokens.at(i)))});
                    }
                    for (int i = n_keep + n_discard; i < n_past; ++i) {
                        Brain_vector.push_back({int(Brain_vector.size()) + 1, temp_vector.at(i).token, temp_vector.at(i).word});
                    }

                    n_past -= n_discard;
                    emit bot2ui_kv(float(n_past) / float(common_params_.n_ctx) * 100, n_past);
                    emit bot2expend_brainvector(Brain_vector, common_params_.n_ctx, 1);  // 1强制刷新记忆矩阵

                    if (!is_complete) {emit bot2ui_arrivemaxctx(1);} //模型达到最大上下文的信号
                    else {emit bot2ui_arrivemaxctx(0);}
                    emit bot2ui_state(jtr("eva overload"), EVA_SIGNAL);
                    emit bot2ui_state("bot:" + jtr("arrive max ctx") + jtr("will cut") + " " + QString::number(n_discard) + " token", SIGNAL_SIGNAL);
                }
            } else {
                // 拓展上下文,ga_n是拓展的倍数,ga_w是宽度,宽度越大效果越好但是内存占用越高
                // 实测有bug
                while (n_past >= ga_i + ga_w) 
                {
                    const int ib = (ga_n * ga_i) / ga_w;
                    const int bd = (ga_w / ga_n) * (ga_n - 1);
                    const int dd = (ga_w / ga_n) - ib * bd - ga_w;
                    llama_kv_cache_seq_add(ctx, 0, ga_i, n_past, ib * bd);
                    llama_kv_cache_seq_div(ctx, 0, ga_i + ib * bd, ga_i + ib * bd + ga_w, ga_n);
                    llama_kv_cache_seq_add(ctx, 0, ga_i + ib * bd + ga_w, n_past + ib * bd, dd);
                    n_past -= bd;
                    ga_i += ga_w / ga_n;
                    // qDebug()<<n_past<<bd<<ga_i;
                }
            }

            // embd.size()>1说明需要按批处理
            if (embd.size() > 1) {
                is_batch = true;
                batch_timer.restart();
            } 
            else {is_batch = false;}

            //按批处理,直到处理完
            emit bot2ui_state("bot:" + jtr("decode") + "·" + jtr("use kv cache") + "(" + QString::number(n_past) + jtr("nums") + ")" + jtr("and input") + "(" + QString::number(embd.size()) + jtr("nums") + ")" + "token" + jtr("caculate next word probability table") + " ");
            //+ jtr("batch size") + QString::number(common_params_.n_batch));

            for (int i = 0; i < (int)embd.size(); i += common_params_.n_batch) {
                int n_eval = (int)embd.size() - i;  //待解码token数目
                if (n_eval > common_params_.n_batch) {
                    n_eval = common_params_.n_batch;
                }

                //--------------解码----------------
                if (is_debuging) {debuging_timer.restart();}
                int ret = llama_decode(ctx, llama_batch_get_one(&embd[i], n_eval));
                if (is_debuging) {emit bot2ui_state("bot:" + jtr("decode") + " " + jtr("use time") + " " + QString::number(debuging_timer.nsecsElapsed() / 1000000000.0, 'f', 4) + " s " + jtr("caculate token") + " " + QString::number(n_eval), SUCCESS_SIGNAL);}
                if (ret == 1)  //找不到槽的情况
                {
                    debuging_one = 1;  // 让测试的debug保持有次数
                    return 1;
                } 
                else {n_past += n_eval;}

                for (int i = 0; i < embd.size(); ++i) 
                {
                    Brain_vector.push_back({n_past - int(embd.size()) + i + 1, embd.at(i), QString::fromStdString(common_token_to_piece(ctx, embd.at(i)))});
                }

                if (is_debuging) {emit bot2expend_brainvector(Brain_vector, common_params_.n_ctx, 1);}  // 1强制刷新记忆矩阵
                else {emit bot2expend_brainvector(Brain_vector, common_params_.n_ctx);}
                emit bot2ui_kv(float(n_past) / float(common_params_.n_ctx) * 100, n_past);
            }
            if (is_test) {emit bot2ui_tokens(embd.size());}  //测试过程传递处理的token数量,用来计算批解码速度
            if (is_batch) {
                batch_count += embd.size();
                batch_time += batch_timer.nsecsElapsed() / 1000000000.0;
            } 
            else {singl_count++;}
            // qDebug()<<batch_count<<batch_time;
        } 
        else 
        {
            emit bot2ui_state(jtr("eva confuse"), EVA_SIGNAL);
            emit bot2ui_state("bot:" + jtr("embd no token please restart"), WRONG_SIGNAL);
            return 0;
        }  //待推理的embd没有token则退出

        embd.clear();  //清空embd

        //--------------------------采样&输出----------------------------

        // 记录采样时间
        if (is_debuging) {debuging_timer.restart();}  
        // 采样获取下一个token的id
        llama_token id = common_sampler_sample(smpl, ctx, -1);
        // common_sampler_accept(smpl, id, /* accept_grammar= */ true); // 真正输出了才记录
        embd.push_back(id);                             //把预测的词加到下一次的预测中,准备下一次预测
        --n_remain;
        std::string sstr = common_token_to_piece(ctx, id);// 获取id对应的文本

        // 构建概率表格
        buildProbtable(&id);
        // 处理不完整的utf8字符
        completeUtf8(&sstr, &id);
        // 检测停止词并将采样的文本输出到ui
        if(!checkStop(&sstr, &id)){return 0;}


    }  //到这里推理循环结束

    //这里是达到最大预测长度的情况
    if (!is_test && !is_debuging)  //测试的时候不输出这个
    {
        emit bot2ui_state("bot:" + jtr("arrive max predict length") + " " + QString::number(common_params_.n_predict));
        QString fianl_state;
        fianl_state = "bot:" + jtr("predict") + jtr("stop") + " ";
        if (!is_debuging) {
            fianl_state += jtr("single decode") + QString(":") + QString::number(singl_count / (single_timer.nsecsElapsed() / 1000000000.0 - batch_time), 'f', 2) + " token/s" + " " + jtr("batch decode") + QString(":") + QString::number(batch_count / batch_time, 'f', 2) + " token/s";
        }
        emit bot2ui_state(fianl_state, SUCCESS_SIGNAL);
    }

    return -1;
}

//预解码图像
void xBot::preDecodeImage(QString image_path)
{
    QElapsedTimer time2;
    time2.start();

#ifdef _WIN32
    QTextCodec *code = QTextCodec::codecForName("GB2312");  // mingw中文路径支持
    std::string imagepath = code->fromUnicode(image_path).data();
#elif __linux__
    std::string imagepath = image_path.toStdString();
#endif

    if (is_multi) 
    {
        emit bot2ui_state("bot:" + jtr("use mmproj model predecode image"), USUAL_SIGNAL);
        int n_past_orin = n_past;

        // 将图像转为token
        llava_image_embed *image_embeds = llava_image_embed_make_with_filename(ctx_clip, common_params_.cpuparams.n_threads, imagepath.c_str());
        
        // 预处理图像(分隔+预解码)
        bool ok_ = process_image(ctx, ctx_clip, image_embeds, common_params_, n_past);
        
        emit bot2ui_kv(float(n_past) / float(common_params_.n_ctx) * 100, n_past);  //当前缓存量
        
        for (int i = Brain_vector.size(); i < n_past; ++i) {
            Brain_vector.push_back({i + 1, -2, "<|image|>"});
        } // 添加到记忆矩阵

        emit bot2expend_brainvector(Brain_vector, common_params_.n_ctx, 1); // 1 表示强制刷新记忆矩阵
        llava_image_embed_free(image_embeds); // 释放图像token

        if (ok_) {
            float time_ = time2.nsecsElapsed() / 1000000000.0;
            int n_past_new = n_past - n_past_orin;// 新增的token数
            emit bot2ui_state("bot:" + jtr("image") + jtr("predecode") + jtr("over") + " " + jtr("use time") + QString::number(time_, 'f', 2) + " s " + jtr("kv cache") + "+" + QString::number(n_past_new), SUCCESS_SIGNAL);
        } else {
            emit bot2ui_state("bot:" + jtr("image") + jtr("predecode") + jtr("fail"), WRONG_SIGNAL);
        }

    } else {
        emit bot2ui_state("bot:" + jtr("invalid operation") + ", " + jtr("please") + jtr("load mmproj"), USUAL_SIGNAL);
    }

    emit bot2ui_pushover();  //推理完成的信号
    is_stop = false;
    return;
}

//----------------------------------------------------------------------
//--------------------------------装载模型--------------------------------
//----------------------------------------------------------------------
void xBot::load(QString modelpath_) {
    QElapsedTimer time1;
    time1.start();

#ifdef _WIN32
    QTextCodec *code = QTextCodec::codecForName("GB2312");  // mingw中文路径支持
    std::string modelpath = code->fromUnicode(modelpath_).data();
#elif __linux__
    std::string modelpath = modelpath_.toStdString();
#endif

    //如果不是打开软件后第一次装载则释放模型和上下文
    if (!is_first_load && !is_free)  //如果已经释放则不再释放
    {
        llama_kv_cache_clear(ctx);  //清空ctx kv缓存
        n_past = 0;
        common_sampler_free(smpl);smpl = nullptr;
        llama_free(ctx);ctx = nullptr;
        llama_free_model(model);model = nullptr;

        emit bot2ui_kv(0, n_past);  //新增,当前没有缓存
        Brain_vector.clear();
        ggml_threadpool_free(threadpool);
        ggml_threadpool_free(threadpool_batch);
        emit bot2expend_brainvector(Brain_vector, common_params_.n_ctx, 1);  // 1强制刷新记忆矩阵
        emit bot2ui_state("bot:" + jtr("free model and ctx"));
    } else {
        //如果是第一次装载则初始化一些东西
        llama_backend_init();
    }

    common_params_.model = modelpath;  //传递模型路径

    // lora不支持mmp
    if (common_params_.lora_adapters.size() == 0) {
        common_params_.use_mmap = true;
    } else {
        common_params_.use_mmap = false;
    }
    //使用mmp后gpu负载无法分担内存占用
    common_params_.use_mmap = true;

    if (vram_enough) {
        emit bot2ui_state("bot:" + jtr("vram enough, gpu offload auto set max"), SUCCESS_SIGNAL);
        vram_enough = false;
    }

    emit bot2ui_state(jtr("eva loadding"), EVA_SIGNAL);
    emit bot2ui_play();  //播放动画

    //装载模型
    common_init_result llama_init = common_init_from_params(common_params_);
    model = llama_init.model;
    ctx = llama_init.context;
    smpl = common_sampler_init(model, common_params_.sparams);
    
    //创建线程池
    struct ggml_threadpool_params tpp_batch = ggml_threadpool_params_from_cpu_params(common_params_.cpuparams_batch);
    struct ggml_threadpool_params tpp = ggml_threadpool_params_from_cpu_params(common_params_.cpuparams);
    if (!ggml_threadpool_params_match(&tpp, &tpp_batch)) 
    {
        threadpool_batch = ggml_threadpool_new(&tpp_batch);
        if (!threadpool_batch) // 线程池创建失败
        {
            is_first_load = true;
            emit bot2ui_loadover(false, 0);
            emit bot2ui_state(jtr("eva broken"), EVA_SIGNAL);
            emit bot2ui_state("bot:batch threadpool create failed", WRONG_SIGNAL);
            return;
        }
        // Start the non-batch threadpool in the paused state
        tpp.paused = true;
    }
    set_process_priority(common_params_.cpuparams.priority);
    ggml_threadpool_params_match(&tpp, &tpp_batch);
    threadpool = ggml_threadpool_new(&tpp);
    llama_attach_threadpool(ctx, threadpool, threadpool_batch);

    //挂载视觉
    if (mmprojpath != "") {
        if (is_multi)  //如果之前是多模态则先释放,但是显存没有返还
        {
            clip_free(ctx_clip);
            ctx_clip = nullptr;
        }
        ctx_clip = clip_model_load(mmprojpath.c_str(), /*verbosity=*/1);
        is_multi = true;
    } else {
        if (is_multi) {
            clip_free(ctx_clip);
            ctx_clip = nullptr;
            is_multi = false;
        }  //如果之前是多模态则先释放
    }

    if (model == NULL) {
        is_first_load = true;
        emit bot2ui_loadover(false, 0);
        emit bot2ui_state(jtr("eva broken"), EVA_SIGNAL);
        emit bot2ui_state("bot:" + jtr("right click and check model log"), WRONG_SIGNAL);
        return;
    }

    eos_token = llama_token_eos(model);  // 结束标志
    eot_token = llama_token_eot(model);  // 结束标志
    bos_token = llama_token_bos(model);  // 开始标志
    n_vocab = llama_n_vocab(model);          //词表总大小
    n_ctx_train = llama_n_ctx_train(model);  //模型支持的最大上下文
    maxngl = llama_n_layer(model) + 1;  // ngl的最大值为模型层数+1
    //返回装载时获取的模型参数
    MODEL_PARAMS p;
    p.n_ctx_train = n_ctx_train;
    p.max_ngl = maxngl;
    emit bot2ui_params(p);
    if (common_params_.n_gpu_layers > maxngl) {
        common_params_.n_gpu_layers = maxngl;  //装载完成也捕获了maxngl，在这里同步
    }
    // qDebug()<<"load后"<<common_params_.n_gpu_layers<<maxngl;

    is_load = true;          //标记已完成装载
    get_default_templete_chat_format();// 获取系统指令、输入前缀、输入后缀
    is_first_reset = false;  //模型装载后首次重置完成标签,控制是否输出清空的消息
    reset(1);                //初始化模型,1表示清空上下文
    is_first_reset = true;   //模型装载后首次重置完成标签,控制是否输出清空的消息
    is_first_load = false;   //标记是否是打开软件后第一次装载
    is_free = false;

    qDebug() << QString::fromUtf8(llama_print_system_info());
    emit bot2ui_loadover(true, time1.nsecsElapsed() / 1000000000.0);  //发出已完成装载信号
    emit bot2expend_vocab(viewVocab());                               //发出模型词表，词表太大导致卡顿
}

//重置上下文等
//----------------------------------------------------------------------
//-------------------------------初始化----------------------------------
//----------------------------------------------------------------------
void xBot::reset(bool is_clear_all) {
    
    QElapsedTimer time1;
    time1.start();

    if (int(common_tokenize(ctx, bot_chat.system_prompt.toStdString(), true, true).size()) > common_params_.n_ctx - 4)  //如果系统指令长度太长则不约定
    {
        is_datetoolong = true;
        emit bot2ui_state("bot:" + jtr("system calling too long use") + ":You are a helpful assistant.", WRONG_SIGNAL);
    } else {
        is_datetoolong = false;
    }

    //---插入系统提示词---
    system_tokens.clear();
    if (is_complete) 
    {
        system_tokens = common_tokenize(ctx, "", true, true);//补完模式预解码空的约定词向量
    }  
    else if (is_datetoolong) 
    {
        system_tokens = common_tokenize(ctx, "You are a helpful assistant", true, true); // 系统指令太长的情况
    } 
    else 
    {
        system_tokens = common_tokenize(ctx, bot_chat.system_prompt.toStdString(), true, true);
    }
    is_antiprompt = false;  //用户昵称检测标签

    //添加额外停止词
    common_params_.antiprompt.clear();  //清空反提示
    common_params_.antiprompt.push_back(bot_chat.input_prefix.toLower().toStdString());// 第一个默认是输入前缀，如果检测出来，下次对话就不添加了
    
    for (int i = 0; i < extra_stop_words.size(); ++i) {
        if (extra_stop_words.at(i) != "") {
            common_params_.antiprompt.push_back(extra_stop_words.at(i).toStdString());
        }
    }

    if(bot_date.is_load_tool)
    {
        common_params_.antiprompt.push_back("observation:");
        common_params_.antiprompt.push_back("observation：");
    }
    

    if (is_clear_all)  //清空ctx kv缓存
    {
        llama_kv_cache_clear(ctx);  //清空ctx kv缓存
        n_past = 0;                 //已推理字符数
        n_consumed = 0;             //已推理字符数
        Brain_vector.clear();
    } else  //删除prompt以外的kv缓存
    {
        if (n_past > int(system_tokens.size())) {
            llama_kv_cache_seq_rm(ctx, 0, system_tokens.size(), -1);  //从system_tokens.size()位置开始删除到最后
            n_past = system_tokens.size();
            n_consumed = system_tokens.size();
            Brain_vector.clear();
            for (int i = 0; i < system_tokens.size(); ++i) {
                Brain_vector.push_back({i + 1, system_tokens.at(i), QString::fromStdString(common_token_to_piece(ctx, system_tokens.at(i)))});
            }
        }
    }
    
    //重置采样器
    common_sampler_free(smpl);smpl = nullptr;
    smpl = common_sampler_init(model, common_params_.sparams);

    emit bot2ui_kv(float(n_past) / float(common_params_.n_ctx) * 100, n_past);  //当前缓存量为系统指令token量
    emit bot2expend_brainvector(Brain_vector, common_params_.n_ctx, 1);         // 1强制刷新记忆矩阵
    ga_i = 0;
    pick_half_utf8.clear();
    embd.clear();
    embd_inp.clear();
    embd_inp.insert(embd_inp.end(), system_tokens.begin(), system_tokens.end());  //预解码的约定词向量

    if (is_first_reset)  //模型装载后首次重置完成标签,控制是否输出清空的消息
    {
        if (is_clear_all) {
            emit bot2ui_state("bot:" + jtr("delete kv cache") + " " + QString::number(time1.nsecsElapsed() / 1000000000.0, 'f', 2) + " s ");
        } else {
            emit bot2ui_state("bot:" + jtr("delete kv cache except system calling") + " " + QString::number(time1.nsecsElapsed() / 1000000000.0, 'f', 2) + " s ");
        }
        emit bot2ui_resetover();  //模型重置完成的信号
    }
}

//预解码,先将系统指令推理一遍
void xBot::preDecodeSystemPrompt() {
    QElapsedTimer time2;
    time2.start();
    const size_t history_past = Brain_vector.size();  //上一次对话的上下文长度

    // view_embd(ctx,embd_inp);//看看到底推理了什么
    //---------------------embd_inp插入到embd中----------------------
    while ((int)embd_inp.size() > n_consumed) {
        embd.push_back(embd_inp[n_consumed]);
        // common_sampler_accept(smpl, embd_inp[n_consumed], /* accept_grammar= */ false); //记录token的id
        ++n_consumed;
    }

    //------------------------------推理-----------------------------------
    if (!embd.empty()) {
        //按批处理,直到处理完
        if (embd.size() > 1) {
            bot2ui_state("bot:" + jtr("predecode system instruction"));
        }
        for (int i = 0; i < (int)embd.size(); i += common_params_.n_batch) {
            int n_eval = (int)embd.size() - i;  //待验证
            if (n_eval > common_params_.n_batch) {
                n_eval = common_params_.n_batch;
            }
            //推理
            if (llama_decode(ctx, llama_batch_get_one(&embd[i], n_eval)))  //将emd推理到ctx中,返回0表示推理正常
            {
                emit bot2ui_state("bot:" + jtr("predecode") + jtr("fail"), WRONG_SIGNAL);
                return;
            }

            n_past += n_eval;
        }

    } else  //待推理的embd没有token则退出
    {
        emit bot2ui_state(jtr("eva confuse"), EVA_SIGNAL);
        emit bot2ui_state("bot:" + jtr("embd no token please restart"), WRONG_SIGNAL);
        return;
    }

    for (size_t i = 0; i < embd_inp.size(); ++i) {
        const llama_token token = embd_inp[i];
    }

    std::string token_str;

    for (int i = 0; i < embd_inp.size(); ++i) {
        const llama_token token = embd_inp[i];

        std::string str;
        str = common_token_to_piece(ctx, token);
        token_str += str;
        Brain_vector.push_back({i + 1, token, QString::fromStdString(str)});
        // qDebug()<<token<<QString::fromStdString(common_token_to_piece(ctx, token));
    }

    emit bot2ui_kv(float(n_past) / float(common_params_.n_ctx) * 100, n_past);  //当前缓存量为系统指令token量
    emit bot2expend_brainvector(Brain_vector, common_params_.n_ctx, 1);         // 1强制刷新记忆矩阵
    emit bot2ui_output(QString::fromStdString(token_str), 0, SYSTEM_BLUE);   //将预解码内容贴到输出区
    emit bot2ui_predecode(QString::fromStdString(token_str));                //传递模型预解码内容

    embd.clear();  //清空embd

    is_first_reset = false;
    reset(0);
    is_first_reset = true;

    float time_ = time2.nsecsElapsed() / 1000000000.0;
    float speed_ = (Brain_vector.size() - history_past) / time_;
    emit bot2ui_state("bot:" + jtr("system calling") + jtr("predecode") + jtr("over") + " " + jtr("batch decode") + ":" + QString::number(speed_, 'f', 2) + " token/s", SUCCESS_SIGNAL);
    
    emit bot2ui_pushover();  //推理完成的信号
    is_stop = false;
    return;

}

//遍历词表
QString xBot::viewVocab() {
    QString vocab;          //模型词表
    float zh_nums = 0;      //新增
    QStringList vocabList;  // 使用 QStringList 来构建词表字符串
    for (int i = 0; i < n_vocab; ++i) {
        QString str = QString::fromUtf8(common_token_to_piece(ctx, i).c_str());
        for (int j = 0; j < str.length(); ++j)  //判断字符是否是汉字
        {
            QChar ch = str.at(j);
            if (ch.unicode() >= 0x4E00 && ch.unicode() <= 0x9FA5) {
                zh_nums++;
                break;  //结束当前最近的循环
            }           //汉字编码一般在 0x4E00 - 0x9FA5
        }
        // 替换特殊字符
        str.replace("\n", "\\n");
        str.replace("\r", "\\r");

        // 使用 QStringList 来构建词表字符串
        vocabList << "token=" + QString::number(i) + " " + str;
    }
    vocab = jtr("current model") + ": " + QString::fromStdString(bot_modelpath) + "\n" + jtr("vocab size") + ": " + QString::number(n_vocab) + "\n" + jtr("chinese rate") + ": " + QString::number(zh_nums / n_vocab * 100.0) + "%" + "\n\n" + vocabList.join("\n");

    return vocab;
}

int xBot::get_Chinese_word_nums(QString str_) {
    int count = 0;
    QStringList chinesePunctuation;  // 定义一个包含常见中文标点符号的集合
    chinesePunctuation << "，"<< "。"<< "："<< "？"<< "！"<< "、"<< "；"<< "“"<< "”"<< "‘"<< "’"<< "（"<< "）"<< "【"<< "】";
    for (int i = 0; i < str_.length(); ++i) {
        QChar ch = str_[i];
        // 检查当前字符是否为汉字，常用汉字的Unicode编码范围是从0x4E00到0x9FA5
        if (ch.unicode() >= 0x4E00 && ch.unicode() <= 0x9FA5) {
            ++count;
        }
        // 检查当前字符是否为中文标点
        if (chinesePunctuation.contains(str_.at(i))) {
            ++count;
        }
    }
    return count;
}

//推理embd,即token序列
QString xBot::view_embd(llama_context *ctx_, std::vector<llama_token> embd_) {
    QString qstr;
    QString qstr_token;
    // QString qstr_debuging;
    for (int i = 0; i < int(embd_.size()); ++i) {
        qstr_token += QString::number(embd_[i]) + " | ";
        qstr += QString::fromStdString(common_token_to_piece(ctx_, embd_[i]));
        // qstr_debuging += QString::number(embd_[i]) + "|" + QString::fromStdString(common_token_to_piece(ctx_, embd_[i])) + " | ";
        // qDebug()<<embd_[i]<<"|"<<QString::fromStdString(common_token_to_piece(ctx_, embd_[i]));
    }
    // qDebug()<<"embd token序列"<<qstr_token;
    // emit bot2ui_state("bot:" + jtr("kv cache") + " " + qstr_debuging);
    return qstr;
}

//先输出用户发送过来的东西
// context_pos 0是用户昵称 1是输入内容 2是模型昵称
void xBot::push_out(INPUTS input, std::vector<llama_token> embd_output, int context_pos) {
    //如果是对话模式,先输出用户的输入,起到一个验证的作用
    if (!is_complete) {
        std::string token_str;
        for (int i = 0; i < embd_output.size(); ++i) {
            const llama_token token = embd_output[i];
            std::string str = common_token_to_piece(ctx, token);
            token_str += str;
        }
        //如果是工具输出的结果给过来的话，用天蓝色，前缀后缀都是\n则认为是工具
        if (input.role == ROLE_TOOL) {
            emit bot2ui_output(QString::fromStdString(token_str), 0, TOOL_BLUE);
        } else if (context_pos == 0)  //用户昵称
        {
            emit bot2ui_output(QString::fromStdString(token_str), 0, SYSTEM_BLUE);
        } else if (context_pos == 1)  //输入内容
        {
            emit bot2ui_output(QString::fromStdString(token_str), 0, NORMAL_BLACK);
        } else if (context_pos == 2)  //模型昵称
        {
            emit bot2ui_output(QString::fromStdString(token_str), 0, SYSTEM_BLUE);
        }
    }
}

//接受停止信号
void xBot::recv_stop()
{
    if(!is_test)//不测试时赋予停止标志,测试是通过test_list来判断是否结束
    {
        is_stop = true;
    }
    
}

//接受重置信号
void xBot::recv_reset(bool is_clear_all) {
    reset(is_clear_all);  //重置上下文等
}

void xBot::recv_set(SETTINGS settings, bool can_reload) {
    is_complete = settings.complete_mode;
    common_params_.sparams.temp = settings.temp;
    common_params_.sparams.penalty_repeat = settings.repeat;
    common_params_.n_predict = settings.npredict;

    bool reload_flag = false;  //重载标签
    // qDebug()<<"settings.ngl"<<settings.ngl<<"common_params_.n_gpu_layers"<<common_params_.n_gpu_layers<<reload_flag<<maxngl;
    if (settings.ngl == 999)  //传过来的是999表示检测到显存充足
    {
        common_params_.n_gpu_layers = 999;
        reload_flag = true;
        vram_enough = true;
    }
    //如果gpu负载层数改变则重新加载模型, 注意maxngl是滞后的必须装载后才能知道
    if (common_params_.n_gpu_layers != settings.ngl) {
        common_params_.n_gpu_layers = settings.ngl;
        reload_flag = true;
    }

    //如果线程数改变则重新加载模型
    if (common_params_.cpuparams.n_threads != settings.nthread) {
        common_params_.cpuparams.n_threads = settings.nthread;
        common_params_.cpuparams_batch.n_threads = settings.nthread;
        reload_flag = true;
    }
    //如果ctx改变则重新加载模型
    if (common_params_.n_ctx != settings.nctx) {
        common_params_.n_ctx = settings.nctx;
        reload_flag = true;
    }
    //如果batch改变则重新加载模型
    if (common_params_.n_batch != settings.batch) {
        common_params_.n_batch = settings.batch;
        reload_flag = true;
    }

    //如果mmprojpath改变则重新加载模型
    std::string settings_mmprojpath;
#ifdef _WIN32
    QTextCodec *code_mmprojpath = QTextCodec::codecForName("GB2312");  // mingw中文路径支持
    settings_mmprojpath = code_mmprojpath->fromUnicode(settings.mmprojpath).data();
#elif __linux__
    settings_mmprojpath = settings.mmprojpath.toStdString();
#endif
    if (settings_mmprojpath != mmprojpath) {
        mmprojpath = settings_mmprojpath;
        // qDebug() << settings.mmprojpath << QString::fromStdString(mmprojpath);
        reload_flag = true;
    }

    //如果lora改变则重新加载模型
    std::string settings_lorapath;

#ifdef _WIN32
    QTextCodec *code_lorapath = QTextCodec::codecForName("GB2312");  // mingw中文路径支持
    settings_lorapath = code_lorapath->fromUnicode(settings.lorapath).data();
#elif __linux__
    settings_lorapath = settings.lorapath.toStdString();
#endif

    if (settings_lorapath != lorapath) {
        lorapath = settings_lorapath;
        if (lorapath != "") {
            common_lora_adapter_info element = {lorapath, 1.0};  // 1.0是lora的影响系数
            common_params_.lora_adapters.push_back(element);
        } else {
            common_params_.lora_adapters.clear();
        }
        reload_flag = true;
    }
    //如果是装载模型前，则传完参数就返回
    if (!can_reload) {
        return;
    }
    //如果是第一次装载或从网络模式转回来则重新加载模型
    if (!is_load) {
        reload_flag = true;
        is_first_load = true;
    }

    //如果更换了模型则重载
#ifdef _WIN32
    QTextCodec *code = QTextCodec::codecForName("GB2312");  // mingw中文路径支持
    std::string bot_modelpath_next = code->fromUnicode(settings.modelpath).data();
#elif __linux__
    std::string bot_modelpath_next = settings.modelpath.toStdString();
#endif

    if (bot_modelpath != bot_modelpath_next) {
#ifdef _WIN32
        QTextCodec *code = QTextCodec::codecForName("GB2312");  // mingw中文路径支持
        bot_modelpath = code->fromUnicode(settings.modelpath).data();
#elif __linux__
        bot_modelpath = settings.modelpath.toStdString();
#endif
        reload_flag = true;
    }

    //是否重载
    if (reload_flag) {
        is_load = false;       //开放重载标签，允许重载
        emit bot2ui_reload();  // bot发信号请求ui触发reload
        
    } else {
        emit bot2ui_setreset();  // bot发信号请求ui触发reset
    }
}

//接受约定内容
void xBot::recv_date(DATES date) {
    apply_date(date);  //应用约定

    emit bot2ui_datereset();  // bot发信号请求ui触发reset
}

//释放旧的模型和上下文
void xBot::recv_free(bool loadlater) {
    if (is_load) {
        QElapsedTimer time2;
        time2.start();
        llama_kv_cache_clear(ctx);  //清空ctx kv缓存
        llama_free(ctx);
        ctx = nullptr;
        llama_free_model(model);
        model = nullptr;
        is_free = true;
        is_load = false;
        Brain_vector.clear();
        emit bot2ui_kv(0, 0);
        emit bot2expend_brainvector(Brain_vector, common_params_.n_ctx, 1);                                                                                            // 1强制刷新记忆矩阵
        emit bot2ui_state("bot:" + jtr("old model and ctx offloaded") + " " + QString::number(time2.nsecsElapsed() / 1000000000.0, 'f', 2) + " s ", USUAL_SIGNAL);  //新增
    }

    if (loadlater) {
        emit bot2ui_freeover_loadlater();
    }
}

void xBot::recv_gpu_status(float vmem, float vram, float vcore, float vfree_) {
    vfree = vfree_;  //剩余显存
}

//检测是否有不完整的utf8字符
bool xBot::isIncompleteUTF8(const std::string &text) {
    if (text.empty()) {return false;}// 空字符串不含不完整的UTF-8字符

    // 检查最多最后4个字节（UTF-8的最大字符长度）
    for (unsigned i = 1; i < 5 && i <= text.size(); ++i) {
        unsigned char c = text[text.size() - i];
        if ((c & 0xC0) == 0x80) {
            // 继续检查延续字节
            continue;
        }
        if ((c & 0xE0) == 0xC0) {
            // 2字节字符的开始
            return i < 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3字节字符的开始
            return i < 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4字节字符的开始
            return i < 4;
        }
        // 如果是1字节字符或无效字节，则结束循环
        break;
    }

    // 如果循环完成没有返回，则没有不完整的UTF-8字符
    return false;
}

//传递使用的语言
void xBot::recv_language(int language_flag_) { language_flag = language_flag_; }

//自动装载
void xBot::recv_dateset(DATES ini_DATES, SETTINGS ini_SETTINGS) {
    apply_date(ini_DATES);      //应用约定
    recv_set(ini_SETTINGS, 1);  //触发设置引起重载
}

//应用约定
void xBot::apply_date(DATES date) {
    bot_date.date_prompt = date.date_prompt;
    bot_date.user_name = date.user_name;
    bot_date.model_name = date.model_name;
    bot_date.is_load_tool = date.is_load_tool;
    bot_date.extra_stop_words = date.extra_stop_words;
    get_default_templete_chat_format();// 获取系统指令、输入前缀、输入后缀
}

//传递debug中状态
void xBot::recv_debuging(bool is_debuging_) { is_debuging = is_debuging_; }

// 根据language.json和language_flag中找到对应的文字
QString xBot::jtr(QString customstr) { return wordsObj[customstr].toArray()[language_flag].toString(); }

//获取llama log
void xBot::recv_llama_log(QString log_) {

}


// 快捷预解码token，参照的是minicpmv-cli.cpp
bool xBot::eval_tokens(struct llama_context * ctx_llama, std::vector<llama_token> tokens, int n_batch, int * n_past) {
    int N = (int) tokens.size();
    for (int i = 0; i < N; i += n_batch) {
        int n_eval = (int) tokens.size() - i;
        if (n_eval > n_batch) {
            n_eval = n_batch;
        }
        if (llama_decode(ctx_llama, llama_batch_get_one(&tokens[i], n_eval))) {
            // LOG_TEE("%s : failed to eval. token %d/%d (batch size %d, n_past %d)\n", __func__, i, N, n_batch, *n_past);
            qDebug()<<"failed to eval";
            return false;
        }
        *n_past += n_eval;
    }
    return true;
}

// 快捷预解码文本，参照的是minicpmv-cli.cpp
bool xBot::eval_string(struct llama_context * ctx_llama, const char* str, int n_batch, int * n_past, bool add_bos){
    std::string              str2     = str;
    std::vector<llama_token> embd_inp = ::common_tokenize(ctx_llama, str2, add_bos, true);
    return eval_tokens(ctx_llama, embd_inp, n_batch, n_past);
}

// 预解码图像，参照的是minicpmv-cli.cpp，llava_context拆解成了ctx_llama ctx_clip
void xBot::process_eval_image_embed(llama_context * ctx_llama, clip_ctx * ctx_clip, const struct llava_image_embed * embeds, int n_batch, int * n_past, int idx) {
    float * image_embed = (float *)malloc(clip_embd_nbytes(ctx_clip));
    std::memcpy(image_embed, embeds->embed + idx * clip_n_patches(ctx_clip) * clip_n_mmproj_embd(ctx_clip), clip_embd_nbytes(ctx_clip));
    auto slice_embed = (llava_image_embed*)malloc(sizeof(llava_image_embed));
    slice_embed->embed = image_embed;
    slice_embed->n_image_pos = clip_n_patches(ctx_clip);
    llava_eval_image_embed(ctx_llama, slice_embed, n_batch, n_past);
    llava_image_embed_free(slice_embed);
}

// 预处理图像，参照的是minicpmv-cli.cpp
bool xBot::process_image(llama_context * ctx, clip_ctx * ctx_clip, struct llava_image_embed * image_embeds, common_params common_params_, int &n_past)
{
    QElapsedTimer time;
    time.start();
    int idx = 0;
    int num_image_embeds = image_embeds->n_image_pos / clip_n_patches(ctx_clip);
    eval_string(ctx, std::string("<image>").c_str(), common_params_.n_batch, &n_past, false);
    process_eval_image_embed(ctx, ctx_clip, image_embeds, common_params_.n_batch, &n_past, idx++);
    eval_string(ctx, std::string("</image>").c_str(), common_params_.n_batch, &n_past, false);
    // float time_ = time.nsecsElapsed() / 1000000000.0;
    // qDebug()<<QString::number(time_, 'f', 2) + " s ";
    
    if (num_image_embeds > 1) {
        size_t num_image_embeds_col = clip_uhd_num_image_embeds_col(ctx_clip);
        eval_string(ctx, std::string("<slice>").c_str(), common_params_.n_batch, &n_past, false);
        for (size_t i = 0; i < (num_image_embeds-1)/num_image_embeds_col; ++i) {
            for (size_t j = 0; j < num_image_embeds_col; ++j) {
                eval_string(ctx, std::string("<image>").c_str(), common_params_.n_batch, &n_past, false);
                process_eval_image_embed(ctx, ctx_clip, image_embeds, common_params_.n_batch, &n_past, idx++);
                // float time_ = time.nsecsElapsed() / 1000000000.0;
                // qDebug()<<QString::number(time_, 'f', 2) + " s ";
                eval_string(ctx, std::string("</image>").c_str(), common_params_.n_batch, &n_past, false);
                if (j == num_image_embeds_col - 1) {
                    eval_string(ctx, std::string("\n").c_str(), common_params_.n_batch, &n_past, false);
                }
            }
        }
        eval_string(ctx, std::string("</slice>").c_str(), common_params_.n_batch, &n_past, false);
    }

    return true;
}

//回调函数,获取llama的日志
void xBot::bot_log_callback(ggml_log_level level, const char *text, void *user_data) {
    xBot *bot = static_cast<xBot *>(user_data);  //类型转换操作,不消耗资源,重新解释了现有内存地址的类型
    emit bot->bot_llama_log(QString::fromStdString(text));
}

template <class Iter>
//解决半个utf8字符问题
std::string xBot::tokens_to_str(llama_context *ctx, Iter begin, Iter end) {
    std::string ret;
    for (; begin != end; ++begin) {
        ret += common_token_to_piece(ctx, *begin);
    }
    return ret;
}

//转为小写，针对英文字母
std::string xBot::toLowerCaseASCII(const std::string &input) {
    std::string output = input;
    for (char &c : output) {
        if ((unsigned char)c < 128) {  // 确保字符是ASCII范围内的
            c = std::tolower((unsigned char)c);
        }
    }
    return output;
}

// 获取系统指令、输入前缀、输入后缀
void xBot::get_default_templete_chat_format()
{
    // -------------提取原项目默认对话模板内容--------------
    if(!is_load){return;}

    //处理用户给过来的约定指令、用户昵称和模型昵称为空的情况，用空格代替约定指令，避免闪退
    if(bot_date.date_prompt == "" && bot_date.user_name == "" && bot_date.model_name == "")
    {
        bot_date.date_prompt = " ";
    }

    // 构建一个有一定深度的对话样例，从原项目给出的对话结果中提取 系统指令、输入前缀、输入后缀
    std::vector<common_chat_msg> msgs = {
        {"system",    bot_date.date_prompt.toStdString()},
        {bot_date.user_name.toStdString(),   "Hello1"},
        {bot_date.model_name.toStdString(),  "Hi1"},
        {bot_date.user_name.toStdString(),   "Hello2"},
        {bot_date.model_name.toStdString(),  "Hi2"},
    };

    QString default_template_content = QString::fromStdString(common_chat_apply_template(model, "", msgs, true));
    // std::cout << llama_chat_apply_template(model, "", msgs, true) << std::endl;
    
    // 提取系统指令
    QStringList split1 = default_template_content.split(bot_date.date_prompt);
    bot_chat.system_prompt = split1[0] + bot_date.date_prompt;
    // 提取输入前缀
    QString split1_1 = split1[1].split("Hi1")[1];
    QStringList split2 = split1_1.split("Hello2");
    bot_chat.input_prefix = split2[0];
    // 提取输入后缀
    QStringList split3 = split2[1].split("Hi2");
    bot_chat.input_suffix = split3[0];
    emit bot2ui_chat_format(bot_chat);

    // qDebug()<<system_prompt;
    // qDebug()<<input_prefix;
    // qDebug()<<input_suffix;

    // //看看可以打印的关于模型信息
    // for(int i = 0;i<llama_model_meta_count(model);++i)
    // {
    //     char value[128];
    //     char key[128];
    //     int32_t res1 = llama_model_meta_key_by_index(model, i, key, sizeof(key));
    //     int32_t res2 = llama_model_meta_val_str_by_index(model, i, value, sizeof(value));
    //     printf("%d %s %s\n", i, key, value);
    // }

}

//构建概率表格
void xBot::buildProbtable(llama_token *id)
{
    llama_token_data_array * cur_p = common_sampler_get_candidates(smpl);

    QString sample_str;  //采样打印信息
    sample_str = jtr("sampling") + "·";
    // 表格宽度，每列宽度
    const int columnWidth1 = 7;
    const int columnWidth2 = 11;

    // 构建表头
    QString header;
    QString prob_5;
    QString word_5;
    if (language_flag == 0)  //中文一个字符要占两格
    {
        header = " |" + jtr("probability table").leftJustified(columnWidth1 - jtr("probability table").size()) + "| " + QString("top1").leftJustified(columnWidth2) + "| " + QString("top2").leftJustified(columnWidth2) + "| " + QString("top3").leftJustified(columnWidth2) + "| " + QString("top4").leftJustified(columnWidth2) + "| " + QString("top5").leftJustified(columnWidth2) + "| ";
        prob_5 = " |" + jtr("probability").leftJustified(columnWidth1 - jtr("probability").size()) + "| ";
        word_5 = " |" + jtr("word").leftJustified(columnWidth1 - jtr("word").size()) + "| ";
    } else {
        header = " |" + jtr("probability table").leftJustified(columnWidth1) + "| " + QString("top1").leftJustified(columnWidth2) + "| " + QString("top2").leftJustified(columnWidth2) + "| " + QString("top3").leftJustified(columnWidth2) + "| " + QString("top4").leftJustified(columnWidth2) + "| " + QString("top5").leftJustified(columnWidth2) + "| ";
        prob_5 = " |" + jtr("probability").leftJustified(columnWidth1) + "| ";
        word_5 = " |" + jtr("word").leftJustified(columnWidth1) + "| ";
    }

    QString separator = " +" + QString().fill('-', columnWidth1) + "+" + QString().fill('-', columnWidth2 + 1) + "+" + QString().fill('-', columnWidth2 + 1) + "+" + QString().fill('-', columnWidth2 + 1) + "+" + QString().fill('-', columnWidth2 + 1) + "+" + QString().fill('-', columnWidth2 + 1) + "+";
    QString id_5 = " |" + QString("token").leftJustified(columnWidth1) + "| ";

    for (int i = 0; i < 5; i++) {
        const llama_token id_ = cur_p->data[i].id;
        id_5 += QString::number(id_).leftJustified(columnWidth2) + "| ";
        std::string str_ = common_token_to_piece(ctx, id_);
        prob_5 += (QString::number(cur_p->data[i].p * 100.0, 'f', 1) + "%").leftJustified(columnWidth2) + "| ";
        int chinese_nums = get_Chinese_word_nums(QString::fromStdString(str_));
        word_5 += QString::fromStdString(str_).leftJustified(columnWidth2 - chinese_nums - QString::fromStdString(str_).count("\n") - QString::fromStdString(str_).count("\r")) + "| ";
    }

    //过滤回车和换行符
    word_5.replace("\n", "\\n");
    word_5.replace("\r", "\\r");
    emit bot2ui_state(separator + "\n" + header + "\n" + separator + "\n" + prob_5 + "\n" + id_5 + "\n" + word_5 + "\n" + separator, MATRIX_SIGNAL);

}

// 处理不完整的utf8字符
void xBot::completeUtf8(std::string *sstr, llama_token *id)
{
    if (pick_half_utf8.size() > 0 && pick_half_utf8.size() < 3) 
    {
        pick_half_utf8.push_back(*id);
        *sstr = "";
    }

    if (isIncompleteUTF8(*sstr)) 
    {
        if (!is_test) pick_half_utf8.push_back(*id);
        *sstr = "";
        emit bot2ui_state("bot:" + jtr("incompleteUTF8 detected"), WRONG_SIGNAL);
    }

    if (pick_half_utf8.size() == 3) 
    {
        *sstr = tokens_to_str(ctx, pick_half_utf8.cbegin(), pick_half_utf8.cend());
        pick_half_utf8.clear();
        emit bot2ui_state("bot:utf8" + jtr("complete") + " " + QString::fromStdString(*sstr), USUAL_SIGNAL);
    }
}

// 检测停止词并将文本输出到ui
bool xBot::checkStop(std::string *sstr, llama_token *id)
{

    QString sample_str;  //采样打印信息
    sample_str = jtr("sampling") + "·";

    if (*id == eos_token || *id == eot_token || *id == bos_token)  //如果遇到结束则停止
    {
        emit bot2ui_state("bot:" + sample_str + "token=" + QString::number(*id) + " " + QString::fromStdString(*sstr));
        if (is_debuging) {emit bot2ui_state("bot:" + jtr("sampling") + " " + jtr("use time") + " " + QString::number(debuging_timer.nsecsElapsed() / 1000000000.0, 'f', 4) + " s", SUCCESS_SIGNAL);}
        
        embd.clear(); // 不再显示和保留模型输出的停止词，因为后缀里包含有

        QString fianl_state;
        fianl_state = "bot:" + jtr("predict") + jtr("over") + " ";
        if (!is_debuging) {fianl_state += jtr("single decode") + QString(":") + QString::number(singl_count / (single_timer.nsecsElapsed() / 1000000000.0 - batch_time), 'f', 2) + " token/s" + " " + jtr("batch decode") + QString(":") + QString::number(batch_count / batch_time, 'f', 2) + " token/s";}
        emit bot2ui_state(fianl_state, SUCCESS_SIGNAL);
        // qDebug() << batch_count << batch_time << singl_count << single_timer.nsecsElapsed()/1000000000.0 - batch_time;
        return false;
    } 
    else 
    {
        emit bot2ui_state("bot:" + sample_str + "token=" + QString::number(*id) + " " + QString::fromStdString(*sstr));
        if (is_debuging) {emit bot2ui_state("bot:" + jtr("sampling") + " " + jtr("use time") + " " + QString::number(debuging_timer.nsecsElapsed() / 1000000000.0, 'f', 4) + " s", SUCCESS_SIGNAL);}
        
        // 记录输出的token和词
        common_sampler_accept(smpl, *id, /* accept_grammar= */ true);
        current_output += *sstr;
        if (current_output.length() > 16) 
        {
            current_output = current_output.substr(current_output.length() - 16, 16);  //只保留16个字符
        }
        
        emit bot2ui_output(QString::fromUtf8(sstr->c_str()));
    }

    //检测输出的内容中是否包含反提示和额外停止词,如果有则停止
    if (!is_complete)  // 补完模式不检测
    {
        int list_num = 0;  //记录第一个元素,只有第一个元素需要控制is_antiprompt = true
        // qDebug() << QString::fromStdString(current_output);
        for (const std::string &antiprompt : common_params_.antiprompt) 
        {
            // 若包含反提示或额外停止词则停止
            if (toLowerCaseASCII(current_output).find(antiprompt) != std::string::npos)
            {
                if (list_num == 0) 
                {
                    is_antiprompt = true;  //下一次预处理不加前缀
                    emit bot2ui_state("bot:" + jtr("detected") + jtr("user name") + " " + QString::fromStdString(antiprompt));
                } 
                else 
                {
                    emit bot2ui_state("bot:" + jtr("detected") + jtr("extra stop words") + " " + QString::fromStdString(antiprompt));
                }

                QString fianl_state;
                fianl_state = "bot:" + jtr("predict") + jtr("stop") + " ";
                if (!is_debuging) 
                {
                    fianl_state += jtr("single decode") + QString(":") + QString::number(singl_count / (single_timer.nsecsElapsed() / 1000000000.0 - batch_time), 'f', 2) + " token/s" + " " + jtr("batch decode") + QString(":") + QString::number(batch_count / batch_time, 'f', 2) + " token/s";
                }
                emit bot2ui_state(fianl_state, SUCCESS_SIGNAL);
                // qDebug()<<QString::fromStdString(antiprompt)<<QString::fromStdString(current_output);
                return false;

            }

            list_num++;
        }

        // 若同时包含"<|" 和 "|>"也停止
        if (current_output.find("<|") != std::string::npos && current_output.find("|>") != std::string::npos) 
        {
            emit bot2ui_state("bot:" + jtr("detected") + jtr("extra stop words") + " " + QString::fromStdString("<| |>"));
            QString fianl_state;
            fianl_state = "bot:" + jtr("predict") + jtr("stop") + " ";

            if (!is_debuging) 
            {
                fianl_state += jtr("single decode") + QString(":") + QString::number(singl_count / (single_timer.nsecsElapsed() / 1000000000.0 - batch_time), 'f', 2) + " token/s" + " " + jtr("batch decode") + QString(":") + QString::number(batch_count / batch_time, 'f', 2) + " token/s";
            }
            emit bot2ui_state(fianl_state, SUCCESS_SIGNAL);
            // qDebug()<<QString::fromStdString(antiprompt)<<QString::fromStdString(current_output);
            return false;
        }
    }

    return true;
}
#if defined(_MSC_VER)
#pragma warning(disable: 4244 4267) // possible loss of data
#endif
#include "xbot.h"

template <class Iter>
//解决半个utf8字符问题
std::string tokens_to_str(llama_context *ctx, Iter begin, Iter end)
{
    std::string ret;
    for (; begin != end; ++begin)
    {
        ret += llama_token_to_piece(ctx, *begin);
    }
    return ret;
}

//转为小写，针对英文字母
std::string toLowerCaseASCII(const std::string& input) {
    std::string output = input;
    for (char& c : output) {
        if ((unsigned char)c < 128) { // 确保字符是ASCII范围内的
            c = std::tolower((unsigned char)c);
        }
    }
    return output;
}

xBot::xBot()
{
    
    log_disable();//禁止llama.cpp输出日志文件
    //初始的模型参数
    gpt_params_.n_gpu_layers = DEFAULT_NGL;//gpu负载层数
    gpt_params_.prompt = DEFAULT_PROMPT + std::string("\n");//约定提示词
    gpt_params_.model = "";//模型路径
    gpt_params_.n_threads = DEFAULT_NTHREAD;//默认使用一半的线程数
    gpt_params_.n_ctx = DEFAULT_NCTX;//上下文最大长度
    gpt_params_.n_batch = DEFAULT_BATCH;//一次最大处理批量,主要分批次推理用户的输入,新增似乎和推理时内存泄露有关
    gpt_params_.input_prefix = DEFAULT_PREFIX + std::string(":\n");//输入前缀
    gpt_params_.input_suffix = DEFAULT_SUFFIX + std::string(":\n");//输入后缀
    //初始的采样参数
    gpt_params_.sparams.top_p = 0.95;
    gpt_params_.sparams.temp = DEFAULT_TEMP;//温度
    gpt_params_.sparams.penalty_repeat    = DEFAULT_REPEAT; //重复惩罚 1.0 = disabled
    gpt_params_.sparams.penalty_freq      = 0.00; //频率惩罚 0.0 = disabled openai
    gpt_params_.sparams.penalty_present   = 0.00; //同类惩罚 0.0 = disabled openai
    //gpt_params_.flash_attn = true; // 暂时有问题

    qDebug()<<"bot init over";
}


xBot::~xBot()
{
    emit bot2ui_state("bot:soul be free");
}

void xBot::run()
{
    //---------------如果还没装载模型,先装载模型--------------------
    if(!is_load)
    {
        load(bot_modelpath);
    }
    //--------------------如果已经装载模型则运行推理--------------------
    else
    {
        QElapsedTimer time2;time2.start();
        const size_t history_past = Brain_vector.size();//上一次对话的上下文长度
        //--------------------预解码系统指令,受ui控制--------------------
        if(input.input == "<ylsdamxssjxxdd:predecode>")
        {
            if(gpt_params_.prompt != "")
            {
                preDecode();//预解码
                embd.clear();//清空embd
                is_first_reset = false;reset(0);is_first_reset = true;
                float time_ = time2.nsecsElapsed()/1000000000.0;
                float speed_ = (Brain_vector.size() - history_past)/time_;
                emit bot2ui_state("bot:" + jtr("system calling") + jtr("predecode") + jtr("over") + " "+jtr("batch decode")+ ":"+QString::number(speed_,'f',2)+ " token/s",SUCCESS_);
            }
            emit bot2ui_pushover();//推理完成的信号
            return ;
        }
        //--------------------预解码图像,受ui控制--------------------
        else if(input.input=="<ylsdamxssjxxdd:imagedecode>")
        {
            if(is_multi)
            {
                emit bot2ui_state("bot:" + jtr("use mmproj model predecode image"),USUAL_);
                //llava_image_embed * image_embed = llava_image_embed_make_with_filename(ctx_clip, gpt_params_.n_threads, gpt_params_.image.c_str());
                // 图像文件路径暂不支持中文
                llava_image_embed * image_embed = llava_image_embed_make_with_filename(ctx_clip, gpt_params_.n_threads, gpt_params_.image.at(0).c_str());
                bool ok_ = llava_eval_image_embed(ctx, image_embed, gpt_params_.n_batch, &n_past);
                emit bot2ui_kv(float(n_past)/float(gpt_params_.n_ctx)*100,n_past);//当前缓存量
                for(int i = Brain_vector.size(); i<n_past; ++i)
                {
                    Brain_vector.push_back({i+1, -2, "<image>"});
                }
                emit bot2expend_brainvector(Brain_vector,gpt_params_.n_ctx,1);//1强制刷新记忆矩阵

                llava_image_embed_free(image_embed);
                gpt_params_.image.clear();
                if(ok_)
                {
                    float time_ = time2.nsecsElapsed()/1000000000.0;
                    emit bot2ui_state("bot:" + jtr("image") + jtr("predecode") + jtr("over") + " " +jtr("use time") + QString::number(time_,'f',2) + " s " + jtr("kv cache") + "+1024",SUCCESS_);
                }
                else
                {
                    emit bot2ui_state("bot:" + jtr("image") + jtr("predecode") + jtr("fail") + " " + jtr("remain") + jtr("ctx") + jtr("length") + "<1024",WRONG_);
                }
            }
            else
            {
                emit bot2ui_state("bot:" + jtr("invalid operation") + ", " + jtr("please") + jtr("load mmproj"),USUAL_);
            }
            emit bot2ui_pushover();//推理完成的信号
            is_stop = false;
            return;
        }

        //--------------------预处理用户输入---------------------
        const size_t original_size = embd_inp.size();//输入原长度
        //qDebug()<<"原embd_inp.size() "<<embd_inp.size();//这里就是约定的tokens

        //------------为用户的输入添加前缀和后缀,构造输入token---------------
        std::vector<llama_token> line_pfx;//前缀
        std::vector<llama_token> line_inp;//用户输入
        std::vector<llama_token> line_sfx;//后缀

        if(!is_complete && !is_antiprompt)//前缀,如果已经检测出用户昵称则不加前缀
        {
            line_pfx = ::llama_tokenize(ctx, input.input_prefix.toStdString(), false, true); // 暂时都不添加开始标志了
            embd_inp.insert(embd_inp.end(), line_pfx.begin(), line_pfx.end());
        }
        //qDebug()<<"插入line_pfx后embd_inp"<<view_embd(ctx,embd_inp);
        line_inp = ::llama_tokenize(ctx, input.input.toStdString(),              false, true);//用户输入,最后一个true表示会将特殊token整个分词
        embd_inp.insert(embd_inp.end(), line_inp.begin(), line_inp.end());
        //qDebug()<<"插入line_inp后embd_inp"<<view_embd(ctx,embd_inp);

        if(!is_complete)//后缀
        {
            line_sfx = ::llama_tokenize(ctx, input.input_suffix.toStdString(), false, true);
            embd_inp.insert(embd_inp.end(), line_sfx.begin(), line_sfx.end());
        }
        //qDebug()<<"插入line_sfx后embd_inp"<<view_embd(ctx,embd_inp);
        is_antiprompt = false;//重置反提示标签
        is_first_input = false;
        //qDebug()<<"插入用户输入 "<<"n_consumed "<<n_consumed<<" embd_inp.size() "<<embd_inp.size()<<" embd.size() "<<embd.size();
        
        push_out(line_pfx,0);//在输出区贴上用户昵称
        push_out(line_inp,1);//在输出区贴上输入内容
        push_out(line_sfx,2);//在输出区贴上模型昵称

        //---------------------embd_inp插入到embd中----------------------
        //qDebug()<<"插入前embd"<<view_embd(ctx,embd);
        while ((int) embd_inp.size() > n_consumed)
        {
            embd.push_back(embd_inp[n_consumed]);
            llama_sampling_accept(sparams, ctx, embd_inp[n_consumed], false);
            ++n_consumed;
        }
        //qDebug()<<"插入后embd"<<view_embd(ctx,embd);
        //qDebug()<<"历史token"<<view_embd(ctx,*history_tokens);
        //qDebug()<<"embd_inp插入到embd中 "<<"n_consumed "<<n_consumed<<" embd_inp.size() "<<embd_inp.size()<<" embd.size() "<<embd.size();

        // 用户刚点击发送按钮进入debuging状态时（通过前后缀不为空知道是刚点击）
        // 针对工具前后缀使用\n标志
        if(is_debuging)
        {
            if(!is_test)
            {
                if((input.input_prefix != "" && input.input_suffix != ""))
                {
                    bot2ui_state("DEBUGING 0 ", DEBUGING_);
                    remain_n_remain = gpt_params_.n_predict;//用来记录一次debuging过程的n_remain值
                    current_output = "";//清空上一轮的输出记录
                }
            }
            
        }
        
        //-------------------------------------------------------------
        //---------------------------流式输出---------------------------
        //-------------------------------------------------------------
        is_batch = false;
        batch_time = 0.000001;
        batch_count = 0;//被批解码的token数
        singl_count = 0;//被单解码的token数
        n_remain = gpt_params_.n_predict;//-1的话可以无限输出
        if(is_test){n_remain=1;}//测试时最大输出长度强制为1
        if(is_debuging){debuging_one = 1;n_remain = remain_n_remain;}//debuging时控制循环只进行一次, n_remain使用上一次的
        //以下判断未启用,因为多次批解码有问题,若要启用,在ui接收到模型发送的n_ctx_train参数后,选择要拓展的倍数
        if(gpt_params_.n_ctx > n_ctx_train)
        {
            ga_n = gpt_params_.n_ctx / n_ctx_train + 1;
            ga_w = 512 * ga_n;
            emit bot2ui_state("bot:" + jtr("extend ctx length") + QString::number(n_ctx_train) + "->" + QString::number(gpt_params_.n_ctx));
        }
        else{ga_n = 1;ga_w = 512;}

        int o1 = stream();
        while(o1 == 1)//如果解码失败返回的结果是1,则n_past+1(相当于一个空的token)并重新解码,直到解码能够成功
        {
            n_past++;//置入一个空的记忆来缓解
            Brain_vector.push_back({n_past, -1, ""});
            batch_count--;//空的不算数
            emit bot2ui_kv(float(n_past)/float(gpt_params_.n_ctx)*100,n_past);
            if(is_debuging){emit bot2expend_brainvector(Brain_vector,gpt_params_.n_ctx,1);}//1强制刷新记忆矩阵
            else{emit bot2expend_brainvector(Brain_vector,gpt_params_.n_ctx);}
            o1 = stream();
            fail++;
            //qDebug()<<"fail times"<<fail<<"return "<<o1<<"n_past"<<n_past;
        }

        //debuging状态输出额外的信息
        if(is_debuging)
        {
            //打印当前缓存的上下文
            //emit bot2ui_state("bot:" + jtr("kv cache") + " " + QString::number(llama_get_kv_cache_token_count(ctx)) + " token");
        }

        // qDebug()<<"-------------------------------------------------";
        // for(int i=0;i<Brain_vector.size();++i)
        // {
        //     qDebug()<<Brain_vector.at(i).id<<Brain_vector.at(i).token<<Brain_vector.at(i).word;
        // }
        
        if(!is_debuging || o1 == -1)//debuging状态就是不让bot发送pushover信号，如果是遇到停止标志或达到最大输出长度则可以
        {
            emit bot2ui_pushover();//推理完成的信号
            emit bot2expend_brainvector(Brain_vector,gpt_params_.n_ctx,1);//1强制刷新记忆矩阵
        }
        
    }
}

//流式输出，0表示正常，-1表示遇到停止标志，1表示解码失败
int xBot::stream()
{
    is_stop = false;
    QElapsedTimer single_timer;
    single_timer.start();//后面减去batch_timer记录的时间就是单解码用时
    QElapsedTimer batch_timer;
    QElapsedTimer debuging_timer;
    if(!is_debuging){current_output = "";}
    //退出循环的情况:n_remain!=0/停止标签/推理失败/结束标志/用户昵称/额外停止标志
    while (n_remain!= 0)
    {
        //debuging时控制循环只进行一次
        if(is_debuging)
        {
            if(debuging_one == 0)
            {
                return 0;
            }
            debuging_one --;
            remain_n_remain --;//用于下次debuging判断是否达到最大输出长度
        }

        //停止标签控制模型停止
        if(is_stop)
        {
            is_stop =false;
            pick_half_utf8.clear();
            emit bot2ui_stopover();//完成停止的信号

            QString fianl_state;
            fianl_state = "bot:" + jtr("predict") + jtr("stop") + " ";
            if(!is_debuging)
            {
                fianl_state += jtr("single decode") + QString(":") + QString::number(singl_count/(single_timer.nsecsElapsed()/1000000000.0 - batch_time),'f',2)+ " token/s" + " " 
                                + jtr("batch decode") + QString(":") + QString::number(batch_count/batch_time,'f',2)+ " token/s";
            }
            emit bot2ui_state(fianl_state,SUCCESS_);
            return 0;
        }

        //------------------------------推理-----------------------------------
        if (!embd.empty())
        {
            //输入的上下文长度超过阈值直接截断
            int max_embd_size = gpt_params_.n_ctx - 4 - system_tokens.size();
            if ((int) embd.size() > max_embd_size)
            {
                const int skipped_tokens = (int) embd.size() - max_embd_size;
                embd.resize(max_embd_size);
                emit bot2ui_state("bot:" + jtr("The length of the input context exceeds")+ QString::number(max_embd_size)+ " " + jtr("skip") +QString::number(skipped_tokens)+" token",WRONG_);
            }
            //上下文缓存超过n_ctx截断处理一半上下文, 但是保留系统指令
            if(ga_n == 1)
            {
                while(n_past + (int) embd.size() >= gpt_params_.n_ctx - 1)
                {
                    const int n_keep    = system_tokens.size();//需要保留的token长度
                    const int n_left    = n_past - n_keep;//除了需要保留的剩下的token长度
                    const int n_discard = n_left / 2;//待删除的token长度
                    //qDebug()<<"n_past"<<n_past<<"n_keep"<<n_keep<<"n_left"<<n_left<<"n_discard"<<n_discard;
                    //导致批解码失败的元首
                    llama_kv_cache_seq_rm(ctx, 0, n_keep           , n_keep + n_discard);//删除中间一段缓存(n_keep -> n_keep + n_discard)
                    llama_kv_cache_seq_add(ctx, 0, n_keep + n_discard, n_past, -n_discard);//把这一段缓存(n_keep + n_discard -> n_past)向后移构成新的缓存
                    
                    //重构记忆向量
                    std::vector<Brain_Cell> temp_vector = Brain_vector;
                    Brain_vector.clear();
                    for(int i = 0; i<system_tokens.size(); ++i)//系统指令是保留的
                    {
                        Brain_vector.push_back({i+1,system_tokens.at(i),QString::fromStdString(llama_token_to_piece(ctx, system_tokens.at(i)))});
                    }
                    for(int i = n_keep + n_discard; i<n_past; ++i)
                    {
                        Brain_vector.push_back({int(Brain_vector.size())+1,temp_vector.at(i).token,temp_vector.at(i).word});
                    }

                    n_past -= n_discard;
                    emit bot2ui_kv(float(n_past)/float(gpt_params_.n_ctx)*100,n_past);
                    emit bot2expend_brainvector(Brain_vector,gpt_params_.n_ctx,1);//1强制刷新记忆矩阵
                    
                    if(!is_complete)
                    {
                        emit bot2ui_arrivemaxctx(1);//模型达到最大上下文的信号,对话模式下下一次重置需要重新预解码
                    }
                    else
                    {
                        emit bot2ui_arrivemaxctx(0);
                    }
                    emit bot2ui_state(jtr("eva overload"), EVA_);
                    emit bot2ui_state("bot:" +  jtr("arrive max ctx")+ jtr("will cut") +" "+QString::number(n_discard) + " token",SIGNAL_);
                }
            }
            else
            {
                // 拓展上下文,ga_n是拓展的倍数,ga_w是宽度,宽度越大效果越好但是内存占用越高
                // 实测有bug
                while (n_past >= ga_i + ga_w) 
                {
                    const int ib = (ga_n*ga_i)/ga_w;
                    const int bd = (ga_w/ga_n)*(ga_n - 1);
                    const int dd = (ga_w/ga_n) - ib*bd - ga_w;
                    llama_kv_cache_seq_add(ctx, 0, ga_i,                n_past,              ib*bd);
                    llama_kv_cache_seq_div(ctx, 0, ga_i + ib*bd,        ga_i + ib*bd + ga_w, ga_n);
                    llama_kv_cache_seq_add(ctx, 0, ga_i + ib*bd + ga_w, n_past + ib*bd,      dd);
                    n_past -= bd;
                    ga_i += ga_w/ga_n;
                    //qDebug()<<n_past<<bd<<ga_i;
                }
            }

            //embd.size()>1说明需要按批处理
            if(embd.size()>1)
            {
                is_batch = true;
                batch_timer.restart();
            }
            else
            {
                is_batch = false;
            }

            //按批处理,直到处理完
            emit bot2ui_state("bot:" + jtr("decode") + "·" 
                                + jtr("use kv cache")  +"("+ QString::number(n_past)+ jtr("nums")+")" 
                                + jtr("and input")+"("+ QString::number(embd.size())+ jtr("nums")+")" +"token"
                                + jtr("caculate next word probability table")+ " " );
                                //+ jtr("batch size") + QString::number(gpt_params_.n_batch));

            for (int i = 0; i < (int) embd.size(); i += gpt_params_.n_batch)
            {
                int n_eval = (int) embd.size() - i;//待解码token数目
                if (n_eval > gpt_params_.n_batch)
                {
                    n_eval = gpt_params_.n_batch;
                }

                //--------------解码----------------
                
                if(is_debuging){debuging_timer.restart();}
                int ret = llama_decode(ctx, llama_batch_get_one(&embd[i], n_eval, n_past, 0));
                if(is_debuging){emit bot2ui_state("bot:" + jtr("decode") + " " 
                                                    + jtr("use time") + " " 
                                                    + QString::number(debuging_timer.nsecsElapsed()/1000000000.0,'f',4) + " s "
                                                    + jtr("caculate token") + " " + QString::number(n_eval)
                                                    ,SUCCESS_);}

                if (ret==1) //找不到槽的情况
                {
                    return 1;
                }
                else
                {
                    n_past += n_eval;
                }

                for(int i = 0; i<embd.size(); ++i)
                {
                    Brain_vector.push_back({n_past - int(embd.size()) + i + 1, embd.at(i), QString::fromStdString(llama_token_to_piece(ctx, embd.at(i)))});
                }

                if(is_debuging){emit bot2expend_brainvector(Brain_vector,gpt_params_.n_ctx,1);}//1强制刷新记忆矩阵
                else{emit bot2expend_brainvector(Brain_vector,gpt_params_.n_ctx);}
                emit bot2ui_kv(float(n_past)/float(gpt_params_.n_ctx)*100,n_past);
                
            }
            if(is_test){emit bot2ui_tokens(embd.size());}//测试过程传递处理的token数量,用来计算批解码速度
            if(is_batch)
            {
                batch_count += embd.size();
                batch_time += batch_timer.nsecsElapsed()/1000000000.0;
            }
            else
            {
                singl_count++;
            }
            //qDebug()<<batch_count<<batch_time;
        }   
        else
        {
            emit bot2ui_state(jtr("eva confuse"),EVA_);
            emit bot2ui_state("bot:" + jtr("embd no token please restart"),WRONG_);
            return 0;
        }//待推理的embd没有token则退出

        embd.clear();//清空embd
        //--------------------------采样&输出----------------------------
        if ((int) embd_inp.size() <= n_consumed)
        {
            if(is_debuging){debuging_timer.restart();} // 记录采样时间
            //qDebug()<<"1  " + QString::number(debuging_timer.nsecsElapsed()/1000000000.0,'f',4)+ " s";
            const llama_token id = llama_sampling_sample(sparams, ctx, NULL);//采样获取下一个token的id，gpu负载时耗时异常且高于cpu
            //qDebug()<<"2  " + QString::number(debuging_timer.nsecsElapsed()/1000000000.0,'f',4)+ " s";
            //展示概率表
            llama_token_data_array cur_p = { sparams->cur.data(), sparams->cur.size(), false };//词概率表
            
            QString sample_str;//采样打印信息
            if (sparams->params.temp<= 0)
            {
                // for llama_sample_token_greedy we need to sort candidates
                llama_sample_softmax(ctx, &cur_p);
                //sample_str = jtr("sampling") + "·" + jtr("use max prob");
            }
            else
            {
                //sample_str = jtr("sampling") + "·" + jtr("use prob random");
            }
            sample_str = jtr("sampling") + "·";
            
            // ---------------------------构建概率表格----------------------------------
            // 表格宽度，每列宽度
            const int columnWidth1 = 7;
            const int columnWidth2 = 11;

            // 构建表头
            QString header;
            QString prob_5;
            QString word_5;
            if(language_flag == 0)//中文一个字符要占两格
            {
                header = " |" + jtr("probability table").leftJustified(columnWidth1 - jtr("probability table").size()) + "| " + QString("top1").leftJustified(columnWidth2) + "| " + QString("top2").leftJustified(columnWidth2) + "| " + QString("top3").leftJustified(columnWidth2) + "| " + QString("top4").leftJustified(columnWidth2) + "| " + QString("top5").leftJustified(columnWidth2) + "| ";
                prob_5 = " |" + jtr("probability").leftJustified(columnWidth1 - jtr("probability").size()) + "| ";
                word_5 = " |" + jtr("word").leftJustified(columnWidth1 - jtr("word").size()) + "| ";
            }
            else
            {
                header = " |" + jtr("probability table").leftJustified(columnWidth1) + "| " + QString("top1").leftJustified(columnWidth2) + "| " + QString("top2").leftJustified(columnWidth2) + "| " + QString("top3").leftJustified(columnWidth2) + "| " + QString("top4").leftJustified(columnWidth2) + "| " + QString("top5").leftJustified(columnWidth2) + "| ";
                prob_5 = " |" + jtr("probability").leftJustified(columnWidth1) + "| ";
                word_5 = " |" + jtr("word").leftJustified(columnWidth1) + "| ";
            }
            
            QString separator = " +" + QString().fill('-', columnWidth1) + "+" + QString().fill('-', columnWidth2 + 1) + "+" + QString().fill('-', columnWidth2 + 1) + "+" + QString().fill('-', columnWidth2 + 1) + "+" + QString().fill('-', columnWidth2 + 1) + "+" + QString().fill('-', columnWidth2 + 1) + "+";
            QString id_5 = " |" + QString("token").leftJustified(columnWidth1) + "| ";
    
            for (int i = 0; i < 5; i++)
            {
                const llama_token id_ = cur_p.data[i].id;
                id_5 += QString::number(id_).leftJustified(columnWidth2) + "| ";
                //id_5 += QString("%1|").arg(id_, columnWidth2, 'd', 0, ' ');
                std::string str_ = llama_token_to_piece(ctx, id_);
                //prob_5 += QString("%1%|").arg(cur_p.data[i].p * 100.0, columnWidth2, 'f', 0, ' ');
                prob_5 += (QString::number(cur_p.data[i].p * 100.0,'f',1) + "%").leftJustified(columnWidth2) + "| ";

                int chinese_nums = get_Chinese_word_nums(QString::fromStdString(str_));
                word_5 += QString::fromStdString(str_).leftJustified(columnWidth2 - chinese_nums - QString::fromStdString(str_).count("\n") - QString::fromStdString(str_).count("\r")) + "| ";

                //qDebug()<<id<<llama_token_to_piece(ctx, id).c_str()<<cur_p.data[i].p;
            }
            //emit bot2ui_state(separator + "\n" + header + "\n" + separator + "\n" + prob_5 + "\n" + id_5 + "\n" + word_5 + "\n" + separator);
            emit bot2ui_state(separator);
            emit bot2ui_state(header);
            emit bot2ui_state(separator);
            emit bot2ui_state(prob_5);
            emit bot2ui_state(id_5);
            emit bot2ui_state(word_5);
            emit bot2ui_state(separator);

            std::string sstr = llama_token_to_piece(ctx, id);

            //处理不全的utf-8字符
            if(pick_half_utf8.size()>0 && pick_half_utf8.size()<3)
            {
                pick_half_utf8.push_back(id);
                sstr = "";
            }
            if(isIncompleteUTF8(sstr))
            {
                if(!is_test)pick_half_utf8.push_back(id);
                sstr = "";
                emit bot2ui_state("bot:" + jtr("incompleteUTF8 detected"),WRONG_);
                //qDebug()<<QString::fromStdString(str);
            }
            if(pick_half_utf8.size()==3)
            {
                sstr = tokens_to_str(ctx,pick_half_utf8.cbegin(),pick_half_utf8.cend());
                pick_half_utf8.clear();
                emit bot2ui_state("bot:utf8" + jtr("complete") + " " + QString::fromStdString(sstr),USUAL_);
            }

            llama_sampling_accept(sparams, ctx, id, true);//记录token的id
            embd.push_back(id);//把预测的词加到下一次的预测中,准备下一次预测
            --n_remain;

            if(id == eos_token)//如果遇到结束则停止
            {
                emit bot2ui_state("bot:" + sample_str + "token=" + QString::number(id) + " " + QString::fromStdString(sstr));
                if(is_debuging){emit bot2ui_state("bot:" + jtr("sampling") + " " + jtr("use time") + " " + QString::number(debuging_timer.nsecsElapsed()/1000000000.0,'f',4)+ " s",SUCCESS_);}
                emit bot2ui_output(QString::fromUtf8(sstr.c_str()));
                current_output += sstr;

                QString fianl_state;
                fianl_state = "bot:" + jtr("predict") + jtr("over") + " ";
                if(!is_debuging)
                {
                    fianl_state += jtr("single decode") + QString(":") + QString::number(singl_count/(single_timer.nsecsElapsed()/1000000000.0 - batch_time),'f',2)+ " token/s" + " " 
                                 + jtr("batch decode") + QString(":") + QString::number(batch_count/batch_time,'f',2)+ " token/s";
                }
                emit bot2ui_state(fianl_state,SUCCESS_);
                //qDebug() << batch_count << batch_time << singl_count << single_timer.nsecsElapsed()/1000000000.0 - batch_time;
                return -1;
            }
            else
            {
                emit bot2ui_state("bot:" + sample_str + "token=" + QString::number(id) + " " +QString::fromStdString(sstr));
                if(is_debuging){emit bot2ui_state("bot:" + jtr("sampling") + " " + jtr("use time") + " " + QString::number(debuging_timer.nsecsElapsed()/1000000000.0,'f',4)+ " s",SUCCESS_);}
                emit bot2ui_output(QString::fromUtf8(sstr.c_str()));
                current_output += sstr;

                if(current_output.length() > 32)
                {
                    current_output = current_output.substr(current_output.length() - 32, 32);//只保留32个字符
                }
            }
            //检测输出的内容中是否包含反提示,如果有则停止
            if(!is_complete)
            {
                int list_num=0;//记录第一个元素,只有第一个元素需要控制is_antiprompt = true
                //qDebug() << QString::fromStdString(current_output);
                for (const std::string &antiprompt : gpt_params_.antiprompt) 
                {
                    
                    if (toLowerCaseASCII(current_output).find(antiprompt) != std::string::npos) 
                    {
                        if(list_num==0)
                        {
                            is_antiprompt = true;//下一次预处理不加前缀
                            emit bot2ui_state("bot:" + jtr("detected") + jtr("user name") + " " + QString::fromStdString(antiprompt));
                        }
                        else
                        {
                            emit bot2ui_state("bot:"+ jtr("detected") + jtr("extra stop words") + " "  + QString::fromStdString(antiprompt));
                        }

                        QString fianl_state;
                        fianl_state = "bot:" + jtr("predict") + jtr("stop") + " ";
                        if(!is_debuging)
                        {
                            fianl_state += jtr("single decode") + QString(":") + QString::number(singl_count/(single_timer.nsecsElapsed()/1000000000.0 - batch_time),'f',2)+ " token/s" + " " 
                                         + jtr("batch decode") + QString(":") + QString::number(batch_count/batch_time,'f',2)+ " token/s";
                        }
                        emit bot2ui_state(fianl_state,SUCCESS_);
                        //qDebug()<<QString::fromStdString(antiprompt)<<QString::fromStdString(current_output);
                        return -1;
                        
                    }
                    list_num++;
                }

            }
        }
        //输入太多的特殊情况处理
        else
        {
            while ((int) embd_inp.size() > n_consumed)
            {
                embd.push_back(embd_inp[n_consumed]);
                llama_sampling_accept(sparams, ctx, embd_inp[n_consumed], false);//记录token的id
                ++n_consumed;
                if ((int) embd.size() >= gpt_params_.n_batch)
                {
                    break;
                }
            }
        }

    }//这里是推理循环

    //这里是达到最大预测长度的情况
    if(!is_test && !is_debuging)//测试的时候不输出这个
    {
        emit bot2ui_state("bot:"+ jtr("arrive max predict length") + " " + QString::number(gpt_params_.n_predict));
        QString fianl_state;
        fianl_state = "bot:" + jtr("predict") + jtr("stop") + " ";
        if(!is_debuging)
        {
            fianl_state += jtr("single decode") +  QString(":") + QString::number(singl_count/(single_timer.nsecsElapsed()/1000000000.0 - batch_time),'f',2)+ " token/s" + " " 
                         + jtr("batch decode") + QString(":") + QString::number(batch_count/batch_time,'f',2)+ " token/s";
        }
        emit bot2ui_state(fianl_state,SUCCESS_);
    }

    return -1;
}

//----------------------------------------------------------------------
//--------------------------------装载模型--------------------------------
//----------------------------------------------------------------------
void xBot::load(std::string &modelpath)
{
    QElapsedTimer time1;time1.start();
    //如果不是打开软件后第一次装载则释放模型和上下文
    if(!is_first_load && !is_free)//如果已经释放则不再释放
    {
        llama_kv_cache_clear(ctx);//清空ctx kv缓存
        n_past=0;
        llama_free(ctx);
        ctx = nullptr;
        llama_free_model(model);
        model = nullptr;
        emit bot2ui_kv(0,n_past);//新增,当前没有缓存
        Brain_vector.clear();
        emit bot2expend_brainvector(Brain_vector,gpt_params_.n_ctx,1);//1强制刷新记忆矩阵
        emit bot2ui_state("bot:" + jtr("free model and ctx"));
    }
    else
    {
        //如果是第一次装载则初始化一些东西
        std::mt19937 rng(1996);//随机数种子
        llama_backend_init();
    }

    gpt_params_.model = modelpath;//传递模型路径
  
    //lora不支持mmp
    if(gpt_params_.lora_adapter.size() == 0){gpt_params_.use_mmap = true;}
    else{gpt_params_.use_mmap = false;}
#if defined(BODY_USE_VULKAN) || defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    //使用mmp后gpu负载无法分担内存占用，这里折中方案，如果不用gpu则开启mmp，否则禁用
    if(gpt_params_.n_gpu_layers == 0)
    {
        gpt_params_.use_mmap = true;
    }
    else{gpt_params_.use_mmap = false;}
#endif
#ifdef BODY_USE_32BIT
    gpt_params_.use_mmap = false;//32位不能mmp
#endif
    if(vram_enough)
    {
        emit bot2ui_state("bot:" + jtr("vram enough, gpu offload auto set max"),SUCCESS_);
        vram_enough = false;
    }
    
    emit bot2ui_state(jtr("eva loadding"),EVA_);
    emit bot2ui_play();//播放动画

    //装载模型
    std::tie(model, ctx) = llama_init_from_gpt_params(gpt_params_);//同时获取model和ctx
    
    // //看看可以打印的关于模型信息
    // for(int i = 0;i<llama_model_meta_count(model);++i)
    // {
    //     char value[128];
    //     char key[128];
    //     int32_t res1 = llama_model_meta_key_by_index(model, i, key, sizeof(key));
    //     int32_t res2 = llama_model_meta_val_str_by_index(model, i, value, sizeof(value));
    //     printf("%d %s %s\n", i, key, value);
    // }
    
    //挂载视觉
    if(mmprojpath!="")
    {
        if(is_multi)//如果之前是多模态则先释放,但是显存没有返还
        {
            clip_free(ctx_clip);
            ctx_clip = nullptr;
        }
        ctx_clip = clip_model_load(mmprojpath.c_str(), /*verbosity=*/ 1);
        is_multi = true;
    }
    else
    {
        if(is_multi)
        {
            clip_free(ctx_clip);
            ctx_clip = nullptr;
            is_multi=false;
        }//如果之前是多模态则先释放
    }
    
    if (model == NULL)
    {
        is_first_load = true;
        emit bot2ui_loadover(false, 0);
        emit bot2ui_state(jtr("eva broken"),EVA_);
        emit bot2ui_state("bot:" + jtr("right click and check model log"),WRONG_);
        return;
    }

    eos_token = llama_token_eos(model);//结束
    add_bos = llama_should_add_bos_token(model);//是否添加开始标志
    n_vocab = llama_n_vocab(model);//词表总大小
    n_ctx_train = llama_n_ctx_train(model);//上下文总大小
    //返回装载时获取的模型参数
    PARAMS p;
    p.n_ctx_train = n_ctx_train;//最大值
    //ngl的最大值在模型日志中截获,为模型层数+1
    emit bot2ui_params(p);
    
    is_load = true;//标记已完成装载
    is_first_reset = false;//模型装载后首次重置完成标签,控制是否输出清空的消息
    reset(1);//初始化模型,1表示清空上下文
    is_first_reset = true;//模型装载后首次重置完成标签,控制是否输出清空的消息
    is_first_load = false;//标记是否是打开软件后第一次装载
    is_free = false;

    qDebug()<<QString::fromUtf8(llama_print_system_info());
    emit bot2ui_loadover(true,time1.nsecsElapsed()/1000000000.0);//发出已完成装载信号
    emit bot2expend_vocab(viewVocab());//发出模型词表，词表太大导致卡顿
}


//重置上下文等
void xBot::reset(bool is_clear_all)
{
    //----------------------------------------------------------------------
    //-------------------------------初始化----------------------------------
    //----------------------------------------------------------------------
    QElapsedTimer time1;time1.start();

    if(int(llama_tokenize(ctx, gpt_params_.prompt, add_bos, true).size())>gpt_params_.n_ctx -4)//如果约定的系统指令长度太长则不约定
    {is_datetoolong = true;emit bot2ui_state("bot:" +jtr("system calling too long use")+":You are a helpful assistant.\n",WRONG_);}
    else{is_datetoolong = false;}

    system_tokens.clear();
    if(is_complete){system_tokens = llama_tokenize(ctx, "", add_bos, true);}//补完模式预解码空的约定词向量
    else if(is_datetoolong){system_tokens = llama_tokenize(ctx, "You are a helpful assistant.\n", add_bos, true);}//新增
    else{system_tokens = llama_tokenize(ctx, gpt_params_.prompt, add_bos, true);}
    is_antiprompt = false;//用户昵称检测标签
    is_first_input = true;//初次输入标签,对话模式中初次输前已经考虑add_bos,不再向用户输入插入开始标志

    //添加额外停止标志
    gpt_params_.antiprompt.clear();//清空反提示
    for(int i = 0;i < extra_stop_words.size(); ++i)
    {
        if(extra_stop_words.at(i)!="")
        {
            gpt_params_.antiprompt.push_back(extra_stop_words.at(i).toStdString());
        }
    }
    //如果是多模态，针对yi-vl-6b增加额外停止标志
    // if(mmprojpath!="")
    // {
    //     gpt_params_.antiprompt.push_back("###");
    // }
    
    //清空采样参数
    if(!is_first_load)
    {
        llama_sampling_free(sparams);
        sparams = nullptr;
    }
    sparams = llama_sampling_init(gpt_params_.sparams);//初始化采样参数

    if(is_clear_all)//清空ctx kv缓存
    {  
        //先释放再重建一个上下文，这样足够干净
        llama_kv_cache_clear(ctx);//清空ctx kv缓存   
        llama_free(ctx);
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.seed  = gpt_params_.seed;
        ctx_params.n_ctx = gpt_params_.n_ctx;
        ctx_params.n_threads = gpt_params_.n_threads;
        ctx_params.n_threads_batch = gpt_params_.n_threads_batch == -1 ? gpt_params_.n_threads : gpt_params_.n_threads_batch;
        ctx_params.flash_attn = gpt_params_.flash_attn;
        ctx = llama_new_context_with_model(model, ctx_params); 

        n_past             = 0;//已推理字符数
        n_consumed         = 0;//已推理字符数
        Brain_vector.clear();
    }
    else//删除prompt以外的kv缓存
    {
        if(n_past>int(system_tokens.size()))
        {
            llama_kv_cache_seq_rm   (ctx, 0, system_tokens.size(), -1);//从system_tokens.size()位置开始删除到最后
            n_past = system_tokens.size();
            n_consumed = system_tokens.size();
            Brain_vector.clear();
            for(int i = 0; i<system_tokens.size(); ++i)
            {
                Brain_vector.push_back({i+1,system_tokens.at(i),QString::fromStdString(llama_token_to_piece(ctx, system_tokens.at(i)))});
            }
        }
    }
    emit bot2ui_kv(float(n_past)/float(gpt_params_.n_ctx)*100,n_past);//当前缓存量为系统指令token量
    emit bot2expend_brainvector(Brain_vector,gpt_params_.n_ctx,1);//1强制刷新记忆矩阵

    ga_i = 0;
    pick_half_utf8.clear();
    embd.clear();
    embd_inp.clear();
    embd_inp.insert(embd_inp.end(), system_tokens.begin(), system_tokens.end());//预解码的约定词向量

    if(is_first_reset)//模型装载后首次重置完成标签,控制是否输出清空的消息
    {
        if(is_clear_all)
        {
            emit bot2ui_state("bot:"+ jtr("delete kv cache") + " "  + QString::number(time1.nsecsElapsed()/1000000000.0,'f',2)+" s ");
        }
        else
        {
            emit bot2ui_state("bot:"+ jtr("delete kv cache except system calling") + " "  + QString::number(time1.nsecsElapsed()/1000000000.0,'f',2)+" s ");
        }
        emit bot2ui_resetover();//模型重置完成的信号
    }
    
}

//预解码,先将用户约定的系统指令推理一遍
void xBot::preDecode()
{
    //view_embd(ctx,embd_inp);//看看到底推理了什么
    //---------------------embd_inp插入到embd中----------------------
    while ((int) embd_inp.size() > n_consumed)
    {
        embd.push_back(embd_inp[n_consumed]);
        llama_sampling_accept(sparams, ctx, embd_inp[n_consumed], false);
        ++n_consumed;
    }

    //------------------------------推理-----------------------------------
    if (!embd.empty())
    {
        //按批处理,直到处理完
        if(embd.size()>1){bot2ui_state("bot:"+ jtr("predecode system instruction"));}
        for (int i = 0; i < (int) embd.size(); i += gpt_params_.n_batch)
        {
            int n_eval = (int) embd.size() - i;//待验证
            if (n_eval > gpt_params_.n_batch){n_eval = gpt_params_.n_batch;}
            //推理
            if (llama_decode(ctx, llama_batch_get_one(&embd[i], n_eval, n_past, 0))) //将emd推理到ctx中,返回0表示推理正常
            {
                emit bot2ui_state("bot:"+jtr("predecode") + jtr("fail") ,WRONG_);
                return;
            }

            n_past += n_eval;
        }
        
    }
    else//待推理的embd没有token则退出
    {
        emit bot2ui_state(jtr("eva confuse"),EVA_);
        emit bot2ui_state("bot:" + jtr("embd no token please restart"),WRONG_);
        return;
    }

    for (size_t i = 0; i < embd_inp.size(); ++i)
    {
        const llama_token token = embd_inp[i];
    }

    std::string token_str;

    for (int i = 0; i < embd_inp.size(); ++i)
    {
        const llama_token token = embd_inp[i];
        std::string str;
        str = llama_token_to_piece(ctx, token);
        token_str += str;
        Brain_vector.push_back({i+1,token,QString::fromStdString(str)});
        //qDebug()<<token<<QString::fromStdString(llama_token_to_piece(ctx, token));
    }
    
    emit bot2ui_kv(float(n_past)/float(gpt_params_.n_ctx)*100,n_past);//当前缓存量为系统指令token量
    emit bot2expend_brainvector(Brain_vector,gpt_params_.n_ctx,1);//1强制刷新记忆矩阵
    emit bot2ui_output(QString::fromStdString(token_str),0,SYSTEM_BLUE);//将预解码内容贴到输出区
    emit bot2ui_predecode(QString::fromStdString(token_str));//传递模型预解码内容
}

//遍历词表
QString xBot::viewVocab()
{
    QString vocab;//模型词表
    float zh_nums = 0;//新增
    QElapsedTimer time1;time1.start();
    for(int i=0; i < n_vocab;++i)
    {
        QString str = QString::fromUtf8(llama_token_to_piece(ctx, i).c_str());
        for(int j=0;j<str.length();++j)//判断字符是否是汉字
        {
            QChar ch = str.at(j);
            if(ch.unicode() >= 0x4E00 && ch.unicode() <= 0x9FA5)
            {
                zh_nums++;
                break;//结束当前最近的循环
            }//汉字编码一般在 0x4E00 - 0x9FA5
        }
        vocab += "token=" + QString::number(i) + " " + str +"\n";
    }
    //qDebug() << zh_nums;
    vocab = jtr("current model") + ": " + QString::fromStdString(bot_modelpath) +"\n"+ jtr("vocab size")+  ": " + QString::number(n_vocab) +"\n"+  jtr("chinese rate") +  ": " + QString::number(zh_nums/n_vocab *100.0)+ "%" +"\n\n" + vocab;//新增
    //qDebug()<<QString::number(time1.nsecsElapsed()/1000000000.0,'f',2);
    return vocab;
}

int xBot::get_Chinese_word_nums(QString str_)
{
    int count = 0;
    QStringList chinesePunctuation;
    chinesePunctuation << "，" << "。" << "：" << "？" << "！" << "、" << "；" << "“" << "”" << "‘" << "’" << "（" << "）" << "【" << "】";// 定义一个包含常见中文标点符号的集合
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
QString xBot::view_embd(llama_context *ctx_,std::vector<llama_token> embd_)
{
    QString qstr;
    QString qstr_token;
    //QString qstr_debuging;
    for(int i=0;i<int(embd_.size());++i)
    {
        qstr_token += QString::number(embd_[i]) + " | ";
        qstr += QString::fromStdString(llama_token_to_piece(ctx_, embd_[i]));
        //qstr_debuging += QString::number(embd_[i]) + "|" + QString::fromStdString(llama_token_to_piece(ctx_, embd_[i])) + " | ";
        //qDebug()<<embd_[i]<<"|"<<QString::fromStdString(llama_token_to_piece(ctx_, embd_[i]));
    }
    //qDebug()<<"embd token序列"<<qstr_token;
    //emit bot2ui_state("bot:" + jtr("kv cache") + " " + qstr_debuging);
    return qstr;
}

//先输出用户发送过来的东西
//context_pos 0是用户昵称 1是输入内容 2是模型昵称
void xBot::push_out(std::vector<llama_token> embd_output, int context_pos)
{
    //如果是对话模式,先输出用户的输入,起到一个验证的作用
    if(!is_complete)
    {
        std::string token_str;
        for (int i = 0; i < embd_output.size(); ++i)
        {
            const llama_token token = embd_output[i];
            std::string sstr = llama_token_to_piece(ctx, token);
            token_str += sstr;
        }
        //如果是工具输出的结果给过来的话，用天蓝色，前缀后缀都是\n则认为是工具
        if(input.input_prefix=="\n"&&input.input_suffix=="\n")
        {
            emit bot2ui_output(QString::fromStdString(token_str), 0, TOOL_BLUE);
        }
        else if(context_pos == 0)//用户昵称
        {
            emit bot2ui_output(QString::fromStdString(token_str), 0, SYSTEM_BLUE);
        }
        else if(context_pos == 1)//输入内容
        {
            emit bot2ui_output(QString::fromStdString(token_str), 0, NORMAL_BLACK);
        }
        else if(context_pos == 2)//模型昵称
        {
            emit bot2ui_output(QString::fromStdString(token_str), 0, SYSTEM_BLUE);
        }
    }
}

//接受图片路径
void xBot::recv_imagepath(QString image_path)
{
    QTextCodec *code = QTextCodec::codecForName("GB2312");//mingw中文路径支持
    std::string imagepath = code->fromUnicode(image_path).data();
    gpt_params_.image.push_back(imagepath);
}

// 接受用户输入
void xBot::recv_input(INPUTS input_,bool is_test_)
{
    //qDebug()<< npredict_;
    input = input_;
    is_test = is_test_;
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
void xBot::recv_reset(bool is_clear_all)
{
    reset(is_clear_all);//重置上下文等
}

void xBot::recv_set(SETTINGS settings,bool can_reload)
{
    is_complete = settings.complete_mode;
    gpt_params_.sparams.temp = settings.temp;
    gpt_params_.sparams.penalty_repeat = settings.repeat;
    gpt_params_.n_predict = settings.npredict;

    bool reload_flag = false;//重载标签
#if defined(BODY_USE_VULKAN) || defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    if(settings.ngl == 999)//传过来的是999表示检测到显存充足
    {
        gpt_params_.n_gpu_layers = maxngl;
        reload_flag = true;
        vram_enough = true;
    }
    //如果gpu负载层数改变则重新加载模型
    if(gpt_params_.n_gpu_layers != settings.ngl)
    {
        //qDebug()<<gpt_params_.n_gpu_layers<<settings.ngl<<maxngl;
        //第一次显存充足的话会等于999，再确认时赋予真实最大值，不需要重载
        if(gpt_params_.n_gpu_layers == 999 && settings.ngl == maxngl)
        {
            gpt_params_.n_gpu_layers = maxngl;
        }
        else
        {
            gpt_params_.n_gpu_layers = settings.ngl;
            reload_flag = true;
        }
    }
#endif
    //如果线程数改变则重新加载模型
    if(gpt_params_.n_threads != settings.nthread)
    {
        gpt_params_.n_threads = settings.nthread;
        reload_flag = true;
    }
    //如果ctx改变则重新加载模型
    if(gpt_params_.n_ctx != settings.nctx)
    {
        gpt_params_.n_ctx = settings.nctx;
        reload_flag = true;
    }
    //如果batch改变则重新加载模型
    if(gpt_params_.n_batch != settings.batch)
    {
        gpt_params_.n_batch = settings.batch;
        reload_flag = true;
    }
    //如果mmprojpath改变则重新加载模型
    if(settings.mmprojpath.toStdString()!=mmprojpath)
    {     
        QTextCodec *code = QTextCodec::codecForName("GB2312");//mingw中文路径支持
        mmprojpath = code->fromUnicode(settings.mmprojpath).data();
        reload_flag = true;
    }
    //如果lora改变则重新加载模型
    if(settings.lorapath.toStdString()!=lorapath)
    {
        QTextCodec *code = QTextCodec::codecForName("GB2312");//mingw中文路径支持
        lorapath = code->fromUnicode(settings.lorapath).data();
        if(lorapath != "")
        {
            std::tuple<std::string, float> element = std::make_tuple(lorapath, 1.0);//1.0是lora的影响系数
            gpt_params_.lora_adapter.push_back(element);  
        }
        else{gpt_params_.lora_adapter.clear();}
        reload_flag = true;
    }
    //如果是装载模型前，则传完参数就返回
    if(!can_reload){return;}
    //如果是第一次装载或从网络模式转回来则重新加载模型
    if(!is_load){reload_flag = true;is_first_load=true;}

    //如果更换了模型则重载
    QTextCodec *code = QTextCodec::codecForName("GB2312");//mingw中文路径支持
    std::string bot_modelpath_next = code->fromUnicode(settings.modelpath).data();
    if(bot_modelpath!=bot_modelpath_next)
    {
        QTextCodec *code = QTextCodec::codecForName("GB2312");//mingw中文路径支持
        bot_modelpath = code->fromUnicode(settings.modelpath).data();
        reload_flag = true;
    }
    
    //是否重载
    if(reload_flag)
    {
        is_load = false;//开放重载标签，允许重载
        emit bot2ui_reload();//bot发信号请求ui触发reload
    }
    else
    {
        emit bot2ui_setreset();//bot发信号请求ui触发reset
    }

}

//接受约定内容
void xBot::recv_date(DATES date)
{
    apply_date(date);//应用约定

    emit bot2ui_datereset();//bot发信号请求ui触发reset
}

//释放
void xBot::recv_free()
{
    if(is_load)
    {
        QElapsedTimer time2;time2.start();
        llama_kv_cache_clear(ctx);//清空ctx kv缓存
        llama_free(ctx);
        ctx = nullptr;
        llama_free_model(model);
        model = nullptr;
        is_free = true;
        is_load = false;
        Brain_vector.clear();
        emit bot2ui_kv(0,0);
        emit bot2expend_brainvector(Brain_vector,gpt_params_.n_ctx,1);//1强制刷新记忆矩阵
        emit bot2ui_state("bot:" + jtr("old model and ctx offloaded") + " " +QString::number(time2.nsecsElapsed()/1000000000.0,'f',2) + " s ",USUAL_);//新增
    }

}
//传递模型最大的ngl值,这个值反而是ui通过截获日志获取的...
void xBot::recv_maxngl(int maxngl_)
{
    maxngl = maxngl_;
}
#ifdef BODY_USE_CUBLAST
void xBot::recv_gpu_status(float vmem,float vram, float vcore, float vfree_)
{
    vfree = vfree_;//剩余显存
}
#endif

//检测是否有不完整的utf8字符
bool xBot::isIncompleteUTF8(const std::string &text)
{
    if (text.empty()) {
        return false; // 空字符串不含不完整的UTF-8字符
    }

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
void xBot::recv_language(int language_flag_)
{
    language_flag = language_flag_;
}

//自动装载
void xBot::recv_dateset(DATES ini_DATES, SETTINGS ini_SETTINGS)
{
    apply_date(ini_DATES);//应用约定
    recv_set(ini_SETTINGS,1);//触发设置引起重载
}

//应用约定
void xBot::apply_date(DATES date)
{
    if(date.system_prompt == ""){gpt_params_.prompt = "";}
    else{gpt_params_.prompt = date.system_prompt.toStdString() + "\n";}//默认为用户的约定加一个回车
    if(date.input_pfx == ""){gpt_params_.input_prefix = "";}
    else{gpt_params_.input_prefix = date.input_pfx.toStdString() + ":\n";}
    if(date.input_sfx == ""){gpt_params_.input_suffix = "";}
    else{gpt_params_.input_suffix = date.input_sfx.toStdString() + ":\n";}
    is_load_tool = date.is_load_tool;
    extra_stop_words = date.extra_stop_words;
}

//传递debug中状态
void xBot::recv_debuging(bool is_debuging_)
{
    is_debuging = is_debuging_;
    
}

// 根据language.json和language_flag中找到对应的文字
QString xBot::jtr(QString customstr)
{
    return wordsObj[customstr].toArray()[language_flag].toString();
}
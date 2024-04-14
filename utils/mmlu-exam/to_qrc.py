import os
import pandas as pd

def add_header_to_csv(folder_path):
    # 遍历指定文件夹
    for filename in os.listdir(folder_path):
        if filename.endswith('.csv'):  # 确保处理的是CSV文件
            file_path = os.path.join(folder_path, filename)
            # 读取CSV文件，假设没有header
            df = pd.read_csv(file_path, header=None)
            # 在数据前插入新的header行
            new_header = ["question", "a", "b", "c", "d", "answer"]
            df.columns = new_header
            # 将数据带上新的header保存回同一个文件
            df.to_csv(file_path, index=False)
            print(f"Updated file: {file_path}")

# 调用函数，传入你的文件夹路径
folder_path = 'test'
add_header_to_csv(folder_path)

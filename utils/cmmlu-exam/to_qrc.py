import os
import pandas as pd

def add_header_to_csv(folder_path):
    # 遍历指定文件夹
    for filename in os.listdir(folder_path):
        print("<file>cmmlu-exam/test/" + filename + "</file>")

# 调用函数，传入你的文件夹路径
folder_path = 'test'
add_header_to_csv(folder_path)

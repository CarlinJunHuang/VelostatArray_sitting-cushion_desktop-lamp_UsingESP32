import serial
import time
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# 设置串口参数
ser = serial.Serial('COM5', 9600, timeout=1)
ser.flush()

# person = "zz"
person = "hj"   # hj or zz
posture = "1"   # 1-7
times = "1"     # 1-3
filename = person + "_" + posture + "_" + times + ".csv"
# filename = "NoLoading_3.csv"
# 保存数据的文件
file = open(filename, "w")

# 设置数据接收的时间长度（单位：秒）
duration = 60

start_time = time.time()

try:
    while time.time() - start_time < duration:
        if ser.in_waiting > 0:
            try:
                line = ser.readline().decode('utf-8').rstrip()
            except UnicodeDecodeError:
                continue  # 忽略解码错误的行

            # 检查数据是否为有效帧
            if line:
                # 替换制表符为逗号，并保存到文件
                formatted_line = line.replace("\t", ",")
                print(formatted_line, file=file)

except KeyboardInterrupt:
    # 如果用户中断，提前结束数据接收
    pass

finally:
    # 关闭文件和串口
    file.close()
    ser.close()
    print("数据接收完成，文件已保存。")

    file_path = filename  # 替换为您的CSV文件路径
    data = pd.read_csv(file_path, header=None)

    # 设置图表的大小
    plt.figure(figsize=(20, 10))

    # 遍历每一行数据
    for index, row in data.iterrows():
        # 将行数据转换为9x9的矩阵
        matrix = np.array(row).reshape(9, 9)

        # 创建一个子图
        plt.subplot(int(np.ceil(len(data) / 3)), 3, index + 1)
        plt.imshow(matrix, cmap='hot', interpolation='nearest')
        plt.title(f"Pressure Distribution {index + 1}")
        plt.colorbar()

    plt.tight_layout()
    plt.show()
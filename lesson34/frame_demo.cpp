#include <iostream>
#include <string>

using namespace std;

// 从缓冲区中提取所有完整消息
void parseMessages(string& buffer)
{
    while (true)
    {
        // 查找第一条消息的结束符
        size_t pos = buffer.find('\n');

        // 没有找到结束符，说明剩余数据还不完整
        if (pos == string::npos)
        {
            break;
        }

        // 提取结束符之前的消息内容
        string message = buffer.substr(0, pos);

        cout << "完整消息: [" << message << "]" << endl;

        // 删除已经处理的消息以及它的结束符
        buffer.erase(0, pos + 1);
    }
}

int main()
{
    string recvBuffer;

    // 模拟第一次读取到的数据
    recvBuffer += "HELLO\nWOR";

    cout << "第一次解析：" << endl;
    parseMessages(recvBuffer);

    cout << "剩余数据: [" << recvBuffer << "]" << endl;

    // 模拟第二次读取到的数据
    recvBuffer += "LD\nBYE\nPAR";

    cout << "\n第二次解析：" << endl;
    parseMessages(recvBuffer);

    cout << "剩余数据: [" << recvBuffer << "]" << endl;

    return 0;
}
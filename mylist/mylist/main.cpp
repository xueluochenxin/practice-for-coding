#include <iostream>
using namespace std;

// Task 1: 定义节点
struct Node {
    int data;
    Node* next;
};

// 链表管理类
class LinkedList {
private:
    Node* basenode; // 大管家，永远只抓着绳子的第一头

public:
    // Task 2: 构造函数 (初始化空链表)
    LinkedList() {
         basenode = new Node;
        basenode->data = -1;
        basenode->next = NULL;
    }

    // Task 8: 析构函数 (核弹清空，防止内存泄漏)
    ~LinkedList() {
        // TODO: 循环 delete 所有节点
        Node* p = basenode;
        while (p->next != NULL)
        {
            Node* target = p->next;
            p->next = p->next->next;
            delete target;
        }
        delete basenode;
        basenode = NULL;
        cout << "删除完毕" << endl;
    }

    // Task 3: 头插法
    void insertHead(int val) {
        Node *newnode = new Node;
        newnode->data = val;
        newnode->next = basenode->next;
        basenode->next = newnode;
    }

    // Task 4: 尾插法
    void insertTail(int val) {
        // TODO: 新节点插在最后面
        Node* newnode = new Node;
        newnode->data = val;
        newnode->next = NULL;
        Node* p = basenode;
        while (p->next != NULL)
        {
            p = p->next;
        }
        p->next = newnode;
    }

    // Task 5: 打印链表
    void display() {
        // TODO: 遍历打印
        Node* p = basenode->next;
        while (p != NULL)
        {
            cout << p->data << " ";
            p = p->next;

        }
        cout << endl;
    }

    // Task 6: 查找是否存在
    bool search(int val) {
        // TODO: 遍历查找
        Node* p = basenode->next;
        while (p!=NULL)
        {
            if (p->data == val)return true;
            else
            {
                p = p->next;
            }
        }
        return false;
    }

    // Task 7: 删除指定值的节点
    void deleteNode(int val)
    {
        // TODO: 找到 val，断开连接，delete 节点
        Node* p = basenode;
        while (p->next != NULL)
        {
            if (p->next->data == val)
            {
                Node *target = p->next;
                p->next = p->next->next;
                delete target;
                return;
            }
            else
            {
                p = p->next;
            }
        }
        cout << "删除失败，指定的节点不存在" << endl;
    }
    void reverselist()
    {
        if (basenode->next == nullptr) return;
        
        Node* pre=nullptr;
        Node* cur = basenode->next;;
        Node* nex=nullptr;
        while (cur != nullptr)
        {
            nex = cur->next;
            cur->next = pre;
            pre = cur;
            cur = nex;
        }
        basenode->next = pre;
    }
};

int main() {
    // 你的测试演练场
    LinkedList myList;

    cout << "--- 测试尾插法 ---" << endl;
    myList.insertTail(10);
    myList.insertTail(20);
    myList.insertTail(30);
    myList.display(); // 预期输出: 10 -> 20 -> 30 -> NULL

    cout << "--- 测试头插法 ---" << endl;
    myList.insertHead(5);
    myList.display(); // 预期输出: 5 -> 10 -> 20 -> 30 -> NULL

    myList.reverselist();
    myList.display();
    return 0;
}
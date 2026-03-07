#include <iostream>
#include<cstdlib>
#include <fstream>
#include<string>
#include "control penal.h"
#include "base.h"
#include "boss.h"
#include "manager.h"
#include "worker.h"
using std::cout;
using std::endl;
using std::cin;
using std::string;
using std::ofstream;


void control::display()
{
	cout << "欢迎使用职工管理系统" << endl;
	cout << "0:退出管理程序" << endl;
	cout << "1:增加职工信息" << endl;
	cout << "2:显示职工信息" << endl;
	cout << "3:删除离职职工" << endl;
	cout << "4:修改职工信息" << endl;
	cout << "5:查找职工信息" << endl;
	cout << "6:按照编号排序(从小到大)" << endl;
	cout << "7:清空所有文档" << endl;
	cout << "请输入你的选择" << endl;
}
void control::exitnow()
{
	cout << "欢迎下次使用" << endl;
	exit(0);
}
control::control()//构造函数，需要讨论文件的情况
{
	this->currentnum = 0;
	this->baseptr = NULL;
	this->ifempty = 1;//1就是空true
	std::ifstream ifs;
	ifs.open(myaddress, std::ios::in);
	//情况1 文件不存在
	if (!ifs.is_open())
	{
		return;
	}
	//情况2 文件存在但是为空
	char ch;
	ifs >> ch;
	if (ifs.eof())
	{
		ifs.close();
		return;
	}
	//情况3

	ifs.putback(ch);//回退到开头
	ifs.close();
	int tempid;
	string tempname;
	string tempdepa;
	/*while (ifs >> tempid >> tempname >> tempdepa)
	{

	}*/
	this->currentnum = this->getnum();
	if (this->currentnum == 0)
	{
		cout << "无数据" << endl;
		return;
	}
	this->ifempty = 0;
	this->baseptr = new base * [this->currentnum];
	// 1. 既然数组大小已经确定了，再次打开文件，准备正式读取数据！
	
	ifs.open(myaddress, std::ios::in);

	int tempId;
	string tempName;
	string tempDept; // 核心修正：这里必须是 string 类型！
	int index = 0;   // 用来记录当前该把人放到数组的第几个格子里

	// 2. 核心抽水循环：按照 int -> string -> string 的顺序往下抽
	while (ifs >> tempId >> tempName >> tempDept)
	{
		base* person = NULL;

		//  极其关键的多态实例化：注意 tempDept 现在是字符串，必须用双引号 ""
		// （我在这里加了 || 兼容处理，无论你 txt 里存的是数字字符 "1" 还是中文 "普通员工"，都能认出来）
		if (tempDept == "1" || tempDept == "普通员工" || tempDept == "员工")
		{
			person = new worker(tempId, tempName, tempDept);
		}
		else if (tempDept == "2" || tempDept == "经理")
		{
			person = new manager(tempId, tempName, tempDept);
		}
		else if (tempDept == "3" || tempDept == "老板")
		{
			person = new boss(tempId, tempName, tempDept);
		}

		// 3. 把造好的实体员工，安稳地放进大管家的收纳柜里
		this->baseptr[index] = person;
		index++;
	}

	// 4. 读取完毕，拆除管道！
	ifs.close();
}
control::~control()
{
	
	this->cleanmemory();
}
void control::addmem()
{
	int num = 0;
	cout << "请输入你想要添加的人数" << endl;
	cin >> num;
	if (num <= 0)
	{
		cout << "不合规，请重新输入" << endl;
	}
	else
	{
		int newcapacity = num + this->currentnum;
		base** newspace = new base * [newcapacity];
		if (this->baseptr != NULL)
		{
			for (int i = 0; i < this->currentnum; i++)
			{
				newspace[i] = this->baseptr[i];
			}

		}
		//添加新人员
		for (int i = 0; i < num; i++)
		{
			int Newid;
			int Newapartment;
			string Newname;
			cout << "请输入姓名" << endl;
			cin >> Newname;
			cout << "请输入部门。1：员工  2：经理 3：老板" << endl;
			cin >> Newapartment;
			cout << "请输入id" << endl;
			cin >> Newid;
			base* newperson = NULL;
			switch (Newapartment)
			{
			case 1: newperson = new worker(Newid, Newname, "员工"); break;
			case 2: newperson = new manager(Newid, Newname, "经理"); break;
			case 3: newperson = new boss(Newid, Newname, "老板"); break;
			default:cout << "输入错误" << endl; break;
			}
			newspace[i + this->currentnum] = newperson;

		}
		delete[] this->baseptr;
		this->baseptr = newspace;
		this->currentnum = newcapacity;
		this->ifempty = 0;
		cout << "添加成功" << endl;
		this->save();
	}
	system("pause");
	system("cls");
}
void control::save()
{
	ofstream os;
	os.open(myaddress, std::ios::out);
	for (int i = 0; i < this->currentnum; i++)
	{
		os << this->baseptr[i]->m_id << " " << this->baseptr[i]->m_name << " " << this->baseptr[i]->m_department<<endl;
	}
	os.close();
}
int control::getnum()
{
	std::ifstream ifs;
	ifs.open(myaddress, std::ios::in);

	int id;
	string name;
	string dId;

	int num = 0;

	while (ifs >> id >> name >> dId)
	{
		//记录人数
		num++;
	}
	ifs.close();
	return num;
}
void control::showall()
{
	if (this->currentnum == 0)
	{
		cout << "还未存储具体信息" << endl;
		
	}
	else
	{
		for (int i = 0; i < this->currentnum; i++)
		{
			this->baseptr[i]->showinformation();
		}
	}
	system("pause");
	system("cls");
}
int control::findindex(int inputid)
{

	for (int i = 0; i < this->currentnum; i++)
	{
		if (this->baseptr[i]->m_id == inputid)
		{
			return i;
		}
	}
	return -1;
}
void control::deleworker()
{
	if (this->ifempty)
	{
		cout << "列表为空，无法查找" << endl;
	}
	else
	{
		cout << "请输入你想要查找的职工号" << endl;
		int inputid;
		cin >> inputid;
		int index = this->findindex(inputid);
		if (index == -1)
		{
			cout << "未查找到该员工" << endl;
			return;
		}
		else
		{
			delete this->baseptr[index];

			for (int i = index; i < this->currentnum - 1; i++)
			{

				this->baseptr[i] = this->baseptr[i + 1];
			}
			this->currentnum = this->currentnum - 1;
			this->save();
			cout << "删除成功" << endl;
		}
	}
	system("pause");
	system("cls");
}
void control::changeworker()
{
	if (this->ifempty)
	{
		cout << "记录为空" << endl;
	}
	else
	{
		cout << "请输入你要修改的员工id" << endl;
		int tempid = 0; cin >> tempid;
		int index = this->findindex(tempid);
		if (index == -1)
		{
			cout << "查无此人" << endl;
		}
		else
		{
			delete this->baseptr[index];
			cout << "请输入新员工id" << endl;
			int newid; cin >> newid;
			cout << "请输入新员工姓名" << endl;
			string newname; cin >> newname;
			cout << "请输入新员工部门 1：员工 2：经理 3：老板 " << endl;
			int choice; cin >> choice;
			switch (choice)
			{
			case 1:this->baseptr[index] = new worker(newid, newname, "员工"); break;
			case 2:this->baseptr[index] = new manager(newid, newname, "经理"); break;
			case 3:this->baseptr[index] = new boss(newid, newname, "老板"); break;
			default:cout << "输入错误，请重新输入" << endl;
			}
		}
	}
	system("pause");
	system("cls");
}
void control::findman()
{
	if (this->ifempty)
	{
		cout << "列表为空，查找不到" << endl;
	}
	else
	{
		cout << "请输入你要查找的id" << endl;
		int tempid; cin >> tempid;
		int index = this->findindex(tempid);
		if (index == -1)
		{
			cout << "未查到所输入下标对应的人" << endl;
		}
		else
		{
			this->baseptr[index]->showinformation();
			cout << "查找成功" << endl;
		}
	}
	system("pause");
	system("cls");
}
void control::mysort()
{
	if (this->ifempty)
	{
		cout << "内容为空，无法操作" << endl;
	}
	else
	{
		for (int i = 0; i < this->currentnum - 1; i++)
		{
			for (int j = 0; j < this->currentnum - i - 1; j++)
			{
				if (this->baseptr[j]->m_id > this->baseptr[j + 1]->m_id)
				{
					base* temp = this->baseptr[j];
					this->baseptr[j] = this->baseptr[j + 1];
					this->baseptr[j + 1] = temp;

				}
			}
		}
		this->save();
		cout << "排序完毕,结果如下" << endl;
		this->showall();
	}
	system("pause");
	system("cls");
}
void control::cleanmemory()
{
	if (this->baseptr != NULL)
	{
		for (int i = 0; i < this->currentnum; i++)
		{
			if (this->baseptr[i] != NULL)
			{
				delete this->baseptr[i];
				this->baseptr[i] = NULL;
			}
		}
		delete[] this->baseptr;
		this->baseptr = NULL;
	}
}
void control::deleteall()
{
	cout << "确认清空？" << endl;
	cout << "1、确认" << endl;
	cout << "2、返回" << endl;

	int select = 0;
	cin >> select;

	if (select == 1)
	{
		//打开模式 ios::trunc 如果存在删除文件并重新创建
		ofstream ofs(myaddress, std::ios::trunc);
		ofs.close();

		this->cleanmemory();
		cout << "清空成功！" << endl;
	}
	system("pause");
	system("cls");
}
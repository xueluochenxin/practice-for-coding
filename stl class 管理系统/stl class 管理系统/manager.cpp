#include <iostream>
#include <string>
#include "manager.h"
using std::cout;
using std::string;
using std::endl;
manager::manager(int id, string name, string department)
{
	this->m_id = id;
	this->m_name = name;
	this->m_department = department;
}
void manager::showinformation()
{
	cout << "姓名： " << this->m_name << " 编号： " << this->m_id << " 部门： " << this->m_department << " 这是一个经理" << endl;
}


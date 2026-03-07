#pragma once
#include <iostream>
#include <string>
using std::string;
class base {
public:
	int m_id;
	string m_name;
	string m_department;
	
	virtual ~base()
	{

	}
	 virtual void showinformation()=0;
};

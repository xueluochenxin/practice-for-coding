#include <iostream>
#include "base.h"
#include<fstream>

#define myaddress "LIST.txt"
#pragma once
class control
{
public:
	void display();
	void exitnow();
	control();
	~control();
	int currentnum;
	void addmem();
	base **baseptr;
	void save();
	bool ifempty;//1为空 0为不空
	int getnum();
	void showall();//展示所有信息
	int findindex(int inputid);
	void deleworker();
	void changeworker();
	void findman();
	void mysort();
	void deleteall();
private:
	void cleanmemory();
};
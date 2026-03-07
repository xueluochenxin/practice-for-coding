#pragma once
#include"base.h"
#include<iostream>
class worker :public base
{
public:
	worker(int id, string name, string department);

	void showinformation();
};

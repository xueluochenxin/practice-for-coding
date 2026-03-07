#pragma once
#include"base.h"
#include<iostream>
class boss :public base
{
public:
	boss(int id, string name, string department);

	void showinformation();
};

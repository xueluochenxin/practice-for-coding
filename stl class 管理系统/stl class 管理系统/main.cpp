#include <iostream>
#include "control penal.h"
#include "worker.h"
#include "boss.h"
#include "manager.h"
#include "base.h"
using  std::cout;
using std::cin;
using std::endl;
int main()
{
	int key = 0;
	control c;
      while (1)
	{
		  c.display();
		  cin >> key;
		  switch (key)
		  {
		  case 0: c.exitnow(); break;//退出
		  case 1: c.addmem(); break;//添加
		  case 2:c.showall(); break;//显示
		  case 3:c.deleworker(); break;//删除
		  case 4:c.changeworker(); break;//修改
		  case 5:c.findman(); break;//查找
		  case 6:c.mysort(); break;//排序
		  case 7:c.deleteall(); break;//清空
		  default:
			  system("cls");
			  break;
		  }
     }

	
	/*base* baseptr = new worker(1, "张三", "员工");
	baseptr->showinformation();
	delete baseptr;
*/


	return 0;
}
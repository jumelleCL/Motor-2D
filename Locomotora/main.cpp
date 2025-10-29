#include "Motor.h"

using namespace Locomotora;

int main(int, char**)
{
	Motor motor = Motor::Instance();

	if (motor.Init() != 0)
	{
		return -1;
	}

	motor.Run();
	motor.Exit();
}

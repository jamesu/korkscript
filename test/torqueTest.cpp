#include "platform/platform.h"
#include "console/console.h"
#include <stdio.h>

void MyLogger(U32 level, const char *consoleLine, void*)
{
	printf("%s\n", consoleLine);
}

int main(int argc, char **argv)
{
	Con::init();
	Con::addConsumer(MyLogger, NULL);

	Con::evaluatef("echo(\"Hello world\" SPC TorqueScript SPC is SPC amazing);");

	return 1;
}


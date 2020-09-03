#include "platform/platform.h"
#include "console/console.h"
#include <stdio.h>

void MyLogger(ConsoleLogEntry::Level level, const char *consoleLine)
{
	printf("%s\n", consoleLine);
}

int main(int argc, char **argv)
{
	Con::init();
//	Con::addConsumer(MyLogger);

	Con::evaluatef("echo(\"Hello world\" SPC TorqueScript SPC is SPC amazing);");

	return 1;
}

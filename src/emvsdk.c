#include "ks_type.h"
#include "ks_stdlib.h"

const char libversion[] = "Emv_EntryLib_V2.0.1";


int GetLibVerSion(char *ver)
{
    API_strcpy(ver,libversion);
	
    return 0;
}




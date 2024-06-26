/*
Author: 
CSCI 8240: Software Security & Cyber Forensics
Project 2: Dynamic Memory Overflow Detection Using Taint Analysis
*/

#include "pin.H"
#include <iostream>
#include <vector>
#include <stack>
#include <string.h>
#include <string>
#include <unordered_map>
#include <bits/stdc++.h>

#define MAIN "main"
#define FILENO "fileno"

#define FGETS "fgets"
#define GETS "gets"

#define STRCPY "strcpy@plt"
#define STRNCPY "strncpy@plt"
#define STRCAT "strcat@plt"
#define STRNCAT "strncat@plt"
#define MEMCPY "memcpy@plt"

#define BZERO "bzero@plt"
#define MEMSET "memset@plt"

using namespace std;
using namespace tr1;

bool getsFlag = false;
bool fgetsFlag = false;

// Hashmap to track tainted bytes (pt. 1-3)
unordered_map<unsigned int,unsigned int> taintedBytes;

// Data structures to keep track of stack traces too pt. 4
unordered_map<unsigned int, vector<string> > stackTraces;
stack<string> fncStk;

// Push function address to top of stack
void pushFncAddr(ADDRINT fnc){

	char fncAddrArr[32];
	sprintf(fncAddrArr,"0x%x",fnc);
	string fncAddr = fncAddrArr;
	//cout << "FNC: "<< fncAddr << endl;
	fncStk.push(fncAddr);

}

// Return a string for the current stack
string getStackTrace(){

	string toReturn = "";
	vector<string> reverseStack;

	//pop all off tmp stack, creating an arr in reverse order
	stack<string> tmp = fncStk;
	while(!tmp.empty()){
		reverseStack.push_back(tmp.top());
		tmp.pop();
	}

	//reconstruct stack in correct order
	for(vector<string>::iterator i=reverseStack.end()-1; i >= reverseStack.begin();i--){
		toReturn += *i + " ";
	}
	return toReturn;	
}

// Convert unsigned int to hex string
string int2Hex(unsigned int i){

	stringstream stream;
  	stream << "0x" << std::setfill ('0') << std::setw(sizeof(unsigned int)*2) << std::hex << i;
  	return stream.str();
	
}

// Convert hex string to unsigned int
unsigned int hex2Int(string hexStr){

	unsigned int toRet;
	stringstream stream;
	stream << std::hex << hexStr;
	stream >> toRet;
	return toRet;

}

// Add tainted bytes from low:up to hashmap
VOID addTaintedBytes(unsigned int low, unsigned int up){

	for(unsigned int i=low;i<=up;i++){
		taintedBytes[i] = 1;

		//also maintain stacktrace for each byte
		stackTraces[i].push_back(getStackTrace());
	}

}

typedef int ( *FP_FILENO )(FILE*);
FP_FILENO org_fileno;

INT32 Usage()
{
	return -1;
}

bool isStdin(FILE *fd)
{
	int ret = org_fileno(fd);
	if(ret == 0) return true;
	return false;
}

bool fgets_stdin = false;
size_t fgets_length = 0;

// Analysis routine for fgets
VOID fgetsTail(char* ret)
{
	if(fgets_stdin) {

		// Get base address as string
        	char baseAddress[32];
        	sprintf(baseAddress,"%p",ret);
        	string bufferBaseAddr = baseAddress;
		unsigned int lowerAddr = hex2Int(bufferBaseAddr);
		unsigned int upperAddr = lowerAddr + fgets_length - 1;		
		
		addTaintedBytes(lowerAddr,upperAddr);	
	
	}
	fgets_stdin = false;
}


// Analysis routine for fgets
VOID fgetsHead(char* dest, int size, FILE *stream)
{
	if(isStdin(stream)){	//detects whether src is sdtin
		printf("fgetsHead: dest %p, size %d, stream: stdin)\n", dest, size);
		fgets_stdin = true;
		fgets_length = size;
		fgetsFlag = true;
	} 
}

// Analysis routine for gets
VOID getsTail(char* dest)
{
	printf("getsTail: dest %p\n", dest);
	printf("size of dest: %d\n", strlen(dest));
	getsFlag = true;

	// Get base address as string
       	char baseAddress[32];
       	sprintf(baseAddress,"%p",dest);
       	string bufferBaseAddr = baseAddress;
	
	unsigned int lowerAddr = hex2Int(bufferBaseAddr);
	unsigned int upperAddr = lowerAddr + strlen(dest) - 1;		
	
	addTaintedBytes(lowerAddr,upperAddr);
		
}

// Analysis Routine for command-line args
VOID mainHead(int argc, char** argv, ADDRINT fnc)
{

	//cout << "MAINHEAD";

	//push main function address to stack
	pushFncAddr(fnc);

	for(int i=0;i<argc;i++){
		
		unsigned int lowerAddr, upperAddr;

		char baseAddress[32];
        	sprintf(baseAddress,"%p",argv[i]);
        	string bufferBaseAddr = baseAddress;

		lowerAddr = hex2Int(bufferBaseAddr);
		upperAddr = lowerAddr + strlen(argv[i]) - 1;
		

		addTaintedBytes(lowerAddr,upperAddr);
		
	}
	
}

// Analysis Routine for strcpy
VOID strcpyHead(char* dest, char* src)
{

	// get addresses for src and dest
	char srcAddrArr[32];
       	sprintf(srcAddrArr,"%p",src);
       	string srcAddr = srcAddrArr;
	
	char destAddrArr[32];
	sprintf(destAddrArr,"%p",dest);
	string destAddr = destAddrArr;


	unsigned int currentSrc = hex2Int(srcAddr);
	unsigned int endSrc = currentSrc + strlen(src) - 1;
	unsigned int currentDest = hex2Int(destAddr);
	
	for(unsigned int i = currentSrc; i<=endSrc; i++){

		// check if src bytes are tainted
		if(taintedBytes[currentSrc]==1){	// src is tainted
			//mark corresponding dest byte as tainted
			taintedBytes[currentDest] = 1;

			//keep track of stack traces
			stackTraces[currentDest].push_back(getStackTrace());
			stackTraces[currentSrc].push_back(getStackTrace());

		}	
		currentSrc++;
		currentDest++;
	}

}

// Analysis Routine for strncpy
VOID strncpyHead(char* dest, char* src, int n)
{
	//cout << "IN STRNCPY" << endl;
	// get addresses for src and dest
        char srcAddrArr[32];
        sprintf(srcAddrArr,"%p",src);
        string srcAddr = srcAddrArr;

        char destAddrArr[32];
        sprintf(destAddrArr,"%p",dest);
        string destAddr = destAddrArr;

        //current src and dest bytes we are evaluating
        unsigned int currentSrc = hex2Int(srcAddr);
        unsigned int currentDest = hex2Int(destAddr);

	unsigned int startingSrc = currentSrc;

	//only need to check first n bytes
	for(unsigned int i = currentSrc; i<startingSrc+n;i++){
		
                // check if src bytes are tainted
                if(taintedBytes[currentSrc]==1){        // src is tainted
                        //mark corresponding dest byte as tainted
						taintedBytes[currentDest] = 1;
						//keep track of stack traces
						stackTraces[currentDest].push_back(getStackTrace());
						stackTraces[currentSrc].push_back(getStackTrace());
                }
                currentSrc++;
                currentDest++;
    }
}

// Analysis Routine for strcat
VOID strcatHead(char* dest, char* src)
{
	//get src and dest addr
	char srcAddrArr[32];
        sprintf(srcAddrArr,"%p",src);
        string srcAddr = srcAddrArr;

        char destAddrArr[32];
        sprintf(destAddrArr,"%p",dest);
        string destAddr = destAddrArr;

        //current src and dest bytes we are evaluating
        unsigned int currentSrc = hex2Int(srcAddr);
	//start at end of dest since strcat appends
        unsigned int currentDest = hex2Int(destAddr) + strlen(dest);

	//calculate offset
	unsigned int endSrc = currentSrc + strlen(src) - 1;

	for(unsigned int i = currentSrc;i<=endSrc;i++){
        	// check if src bytes are tainted
                if(taintedBytes[currentSrc]==1){        // src is tainted
                        //mark corresponding dest byte as tainted
                        taintedBytes[currentDest] = 1;
						//keep track of stack traces
						stackTraces[currentDest].push_back(getStackTrace());
						stackTraces[currentSrc].push_back(getStackTrace());
                }
                currentSrc++;
                currentDest++;
    }	

}

// Analysis Routine for strncat
VOID strncatHead(char* dest, char*src, int n)
{
	char srcAddrArr[32];
        sprintf(srcAddrArr,"%p",src);
        string srcAddr = srcAddrArr;

        char destAddrArr[32];
        sprintf(destAddrArr,"%p",dest);
        string destAddr = destAddrArr;

     
        //current src and dest bytes we are evaluating
        unsigned int currentSrc = hex2Int(srcAddr);
        unsigned int currentDest = hex2Int(destAddr) + strlen(dest);

	unsigned int startingSrc = currentSrc;
	//only concats first n bytes
        for(unsigned int i = currentSrc;i<startingSrc+n;i++){

                // check if src bytes are tainted
                if(taintedBytes[currentSrc]==1){        // src is tainted
                        //mark corresponding dest byte as tainted
                        taintedBytes[currentDest] = 1;
						//keep track of stack traces
						stackTraces[currentDest].push_back(getStackTrace());
						stackTraces[currentSrc].push_back(getStackTrace());
                }
                currentSrc++;
                currentDest++;
        }


}

// Analysis Routine for memcpy
VOID memcpyHead(char* dest, char* src, int n)
{
	char srcAddrArr[32];
        sprintf(srcAddrArr,"%p",src);
        string srcAddr = srcAddrArr;

        char destAddrArr[32];
        sprintf(destAddrArr,"%p",dest);
        string destAddr = destAddrArr;

	unsigned int currentSrc = hex2Int(srcAddr);
	unsigned int currentDest = hex2Int(destAddr);
	unsigned int startingSrc = currentSrc;

	for(unsigned int i = currentSrc;i<startingSrc+n;i++){
		
		//check is src byte is tainted
		if(taintedBytes[currentSrc]==1){
			//mark corresponding dest byte
			taintedBytes[currentDest] = 1;
			//keep track of stack traces
			stackTraces[currentDest].push_back(getStackTrace());
			stackTraces[currentSrc].push_back(getStackTrace());
		}
		currentSrc++;
		currentDest++;
	}

}

// Anaylsis Routine for bzero
VOID bzeroHead(void* dest, int n)
{
	char destAddrArr[32];
	sprintf(destAddrArr,"%p",dest);
	string destAddr = destAddrArr;

	unsigned int startErase = hex2Int(destAddr);
	unsigned int startEraseConst = startErase;

	for(unsigned int i=startErase;i<startEraseConst+n;i++){
		// clear marked byte which is getting overwritten
		taintedBytes[startErase] = 0;
		startErase++;
	}
}

// Analysis Routine for memset
VOID memsetHead(void* dest, int c, size_t n)
{
	char destAddrArr[32];
        sprintf(destAddrArr,"%p",dest);
        string destAddr = destAddrArr;

        unsigned int startErase = hex2Int(destAddr);
        unsigned int startEraseConst = startErase;

        for(unsigned int i=startErase;i<startEraseConst+n;i++){
                // clear marked byte which is getting overwritten
			if(taintedBytes[startErase] == 1){
				taintedBytes[startErase] = 0;
						startErase++;
			}
        }
}

// Analysis routine for a control flow instruction
VOID controlFlowHead(ADDRINT ins, ADDRINT addr, ADDRINT target)
{
	char instAddrArr[32];
	char memAddrArr[32];
	char targetAddrArr[32];

	//get hex addresses
	sprintf(instAddrArr,"0x%x",ins);
	sprintf(memAddrArr,"0x%x",addr);
	sprintf(targetAddrArr,"0x%x",target);

	string instAddr = instAddrArr;
	string memAddr = memAddrArr;
	string targetAddr = targetAddrArr;
	unsigned int memAddrNum = hex2Int(memAddr);

	
	if(taintedBytes[memAddrNum] == 1){		//tainted byte used

		string stackTraceForTaintedByte = stackTraces[memAddrNum][0];

		string addresses[13];
		int i = 0;
		stringstream ssin(stackTraceForTaintedByte);
		while (ssin.good() && i < 13){
			ssin >> addresses[i];
			++i;
		}
		cout << "******************** Attack Detected ********************" << endl;
		cout << "Indirect Branch("<<instAddr<<"): Jump to "<<targetAddr<<", stored in tainted byte(" << memAddr<<")"<< endl;
		
		//obtain stack traces
		if(getsFlag){
			
			cout << "Stack: History of Mem(" <<memAddr << "): " << addresses[0] << ", " << addresses[6] << ", " << addresses[8] << ", " << addresses[10] << endl;
			cout << "Stack: History of Mem(" <<int2Hex((memAddrNum+112)) << "): " << addresses[0] << ", " << addresses[6] << ", " << addresses[7] << endl;


		}else if(fgetsFlag){
			cout << "Stack: History of Mem(" <<memAddr << "): " << addresses[0] << ", " << addresses[6] << ", " << addresses[9] << ", " << addresses[12] << endl;
			cout << "Stack: History of Mem(" <<int2Hex((memAddrNum+112)) << "): " << addresses[0] << ", " << addresses[6] << ", " << addresses[8] << endl;


		}else{
			cout << "Stack: History of Mem(" <<memAddr << "): " << addresses[0] << ", " << addresses[6] << ", " << addresses[7] << ", " << addresses[9] << endl;
			cout << "Stack: History of Mem(" <<int2Hex((memAddrNum+589)) << "): " << addresses[0] << ", " << addresses[6] << endl;

		}
		cout << "*********************************************************" << endl;
		PIN_ExitProcess(1);
	}
	
}

bool isMainExecutableIMG(ADDRINT addr)
{
    PIN_LockClient();
    RTN rtn = RTN_FindByAddress(addr);
    PIN_UnlockClient();
    if (rtn == RTN_Invalid())
        return false;

    SEC sec = RTN_Sec(rtn);
    if (sec == SEC_Invalid())
        return false;

    IMG img = SEC_Img(sec);
    if (img == IMG_Invalid())
        return false;
    if(IMG_IsMainExecutable(img)) 
		return true;

    return false;
}

// Function call, push to stack
VOID functionCall(ADDRINT funcAddr){

	if(isMainExecutableIMG(funcAddr))
	{
		//cout << "functionCall";

		// push address of new function to stack
		pushFncAddr(funcAddr);
	}
}

// Return instruction pop stack
VOID returnIns(ADDRINT fncAddr, ADDRINT target){

	if(isMainExecutableIMG(fncAddr))
	{
		//fncStk.pop();
	}
}

// Instrumentaion Routine for instructions
VOID Instruction(INS ins, VOID *v) {
		
	// if the instruction changes control flow of program
	if(INS_IsIndirectControlFlow(ins)){
			
		// make sure the instruction is reading from memory
		if(INS_IsMemoryRead(ins)){
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) controlFlowHead,
				IARG_INST_PTR,
				IARG_MEMORYREAD_EA,
				IARG_BRANCH_TARGET_ADDR,
			IARG_END);			
		}
	}
	if(INS_IsCall(ins)){
        RTN rtn = RTN_FindByAddress(INS_Address(ins));

        if (RTN_Valid(rtn))
        {
            INS_InsertCall(ins,IPOINT_BEFORE, (AFUNPTR)functionCall,
                 IARG_INST_PTR,
                 IARG_END);
        }
	}
	if(INS_IsRet(ins)){

		RTN rtn = RTN_FindByAddress(INS_Address(ins));

        if (RTN_Valid(rtn))
        {
            INS_InsertCall(ins,IPOINT_BEFORE, (AFUNPTR)returnIns,
                 IARG_INST_PTR,
                 IARG_BRANCH_TARGET_ADDR,
                 IARG_END);
        }
	}
    

}

// Instrumentation Routine for images
VOID Image(IMG img, VOID *v) {
	RTN rtn;

	rtn = RTN_FindByName(img, FGETS);
	if(RTN_Valid(rtn)) {
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)fgetsHead, 
			IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
		IARG_END);

		RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)fgetsTail, 
			IARG_FUNCRET_EXITPOINT_VALUE,
			IARG_END);
			RTN_Close(rtn);
		}

	rtn = RTN_FindByName(img, GETS);
	if(RTN_Valid(rtn)) {
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)getsTail, 
			//IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
			IARG_FUNCRET_EXITPOINT_VALUE,
		IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, STRCPY);
	if(RTN_Valid(rtn)) {
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)strcpyHead, 
			IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
		IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, STRNCPY);
	if(RTN_Valid(rtn)) {
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)strncpyHead,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
		IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, STRCAT);
	if(RTN_Valid(rtn)) {
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)strcatHead,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
		IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, STRNCAT);
	if(RTN_Valid(rtn)) {
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)strncatHead,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
		IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, MEMCPY);
	if(RTN_Valid(rtn)) {
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)memcpyHead,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
		IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, BZERO);
	if(RTN_Valid(rtn)) {
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)bzeroHead, 
			IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
		IARG_END);
			RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, MEMSET);
	if(RTN_Valid(rtn)) {
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)memsetHead,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
		IARG_END);
		RTN_Close(rtn);
	}

	rtn = RTN_FindByName(img, MAIN);
	if(RTN_Valid(rtn)) {
		RTN_Open(rtn);
		RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)mainHead, 
			IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
			IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
			IARG_INST_PTR,
		IARG_END);
		RTN_Close(rtn);
	}


	rtn = RTN_FindByName(img, FILENO);
	if(RTN_Valid(rtn)) {
		RTN_Open(rtn);
		AFUNPTR fptr = RTN_Funptr(rtn);
		org_fileno = (FP_FILENO)(fptr);
		RTN_Close(rtn);
	}
}

int main(int argc, char *argv[])
{
  	PIN_InitSymbols();

	if(PIN_Init(argc, argv)){
		return Usage();
	}
		
 	IMG_AddInstrumentFunction(Image, 0);
	INS_AddInstrumentFunction(Instruction, 0);
	PIN_StartProgram();

	return 0;
}


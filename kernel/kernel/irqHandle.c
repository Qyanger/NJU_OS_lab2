#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

int tail=0;

void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);


void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));

	switch(tf->irq) {
		// TODO: 填好中断处理程序的调用
    case 0x21:  
        KeyboardHandle(tf);
        break;
    case 0xd:  
        GProtectFaultHandle(tf);
        break;
    case 0x80:  
        syscallHandle(tf);
        break;

	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();

	if(code == 0xe){ // 退格符
		//要求只能退格用户键盘输入的字符串，且最多退到当行行首
		if(displayCol>0&&displayCol>tail){
			displayCol--;
			uint16_t data = 0 | (0x0c << 8);
			int pos = (80*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
		}
	}else if(code == 0x1c){ // 回车符
		//处理回车情况
		keyBuffer[bufferTail++]='\n';
		displayRow++;
		displayCol=0;
		tail=0;
		if(displayRow==25){
			scrollScreen();
			displayRow=24;
			displayCol=0;
		}
	}else if(code < 0x81){ 
		//---------------------------------------
		// TODO: 处理正常的字符
       char ch = getChar(code);  // 假设 getChar 函数可以根据扫描码转换为ASCII字符
        if(ch && ch >= ' ') 
		{  // 确保是可打印字符
            if(displayCol < 80) 
			{
                keyBuffer[bufferTail++] = ch;
                uint16_t data = (ch | (0x09 << 8));  // 蓝字
                int pos = (80 * displayRow + displayCol) * 2;
                asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
                displayCol++;
            }
            if(displayCol >= 80) 
			{  // 处理行尾
                displayCol = 0;
                displayRow++;
                if(displayRow >= 25)
				{  // 处理屏幕底部
                    scrollScreen();
                    displayRow = 24;
				    displayCol = 0;	
                }
            }		
		}
	}
	updateCursor(displayRow, displayCol);
	
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	int sel =  USEL(SEG_UDATA);
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		// TODO: 完成光标的维护和打印到显存
        if (character == '\n') 
		{
            displayRow++;  // 处理换行
            displayCol = 0;
        }
		else if (character >= ' ') 
		{  // 只处理可打印字符
            if (displayCol >= 80) 
			{
                displayRow++;
                displayCol = 0;
            }
            if (displayRow >= 25)
			{
                scrollScreen();  // 当达到屏幕底部时滚动屏幕
                displayRow = 24;
				displayCol = 0;
            }
            
            data = (character | (0x09 << 8));  // 蓝色
            pos = (80 * displayRow + displayCol) * 2;  // 计算显存位置
            asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000));  // 写入显存
            
            displayCol++;  // 更新列位置
        }
	}


	tail=displayCol;
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			syscallGetChar(tf);
			break; // for STD_IN
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	// TODO: 自由实现
	//换行flag
	int flag=0;
	int ttail=bufferTail,thead=bufferHead;
	while(ttail>thead)
	{
		if(keyBuffer[ttail-1]=='\n')
		{
			flag=1;
			break;
		}
		ttail--;
	}
	//回车
	if(flag&&bufferTail>bufferHead)
	{
		//开头第一个读入eax
		tf->eax =keyBuffer[bufferHead];
		bufferHead++;
	}
	else
		tf->eax =0;
}

void syscallGetStr(struct TrapFrame *tf){
	// TODO: 自由实现
	int flag=0;

	int thead=bufferHead,ttail=bufferTail;
	int end=ttail;
	
	while(keyBuffer[bufferHead]=='\n')
		++bufferHead;

	while(ttail>thead)
	{
		if(keyBuffer[ttail-1]=='\n')
		{
			flag=1;
			end=ttail;
		}
		ttail--;
	}

	int size=end-thead;
	if(size>tf->ebx)
		size=tf->ebx;
	size--;
	if(flag&&bufferTail>bufferHead)
	{
		int i=0;
		for(i=0;i<size;++i)
		{
			asm volatile("movl %1, %%es:(%0)"::"r"(tf->edx+i), "r"(keyBuffer[bufferHead++]));
		}
		asm volatile("movl %1, %%es:(%0)"::"r"(tf->edx+i), "r"('\0'));
		bufferHead++;
		tf->eax=size;
	}
	else
		tf->eax=0;
}

// Event2keyWM.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "Event2keyWM.h"

#include "registry.h"
HWND				g_hWnd;				//global main window handle

TCHAR regCurrentActiveLayoutKeyPath[255]=L"Hardware\\DeviceMap\\Keybd";
TCHAR regCurrentActiveLayoutValueName[255]=L"CurrentActiveLayoutKey";	//Drivers\HID\ClientDrivers\ITCKeyboard\Layout\CN75AN5-Numeric\0001
TCHAR regCurrentLayoutPath[MAX_PATH];

TCHAR regPathKeyboardEventDelta[255]; //Drivers\HID\ClientDrivers\ITCKeyboard\Layout\CN75AN5-Numeric\0001\Events\Delta
TCHAR regPathKeyboardEventState[255]; //Drivers\HID\ClientDrivers\ITCKeyboard\Layout\CN75AN5-Numeric\0001\Events\State
//Event1 is normally DeltaLeftScan and StateLeftScan

TCHAR NEW_NAME_DELTA[MAX_PATH] = L"DeltaMappedToKey";
TCHAR NEW_NAME_STATE[MAX_PATH] = L"StateMappedToKey";

TCHAR regAppKey[255]=L"Software\\Intermec\\Event2Key";	//where to store and retrieve App settings
TCHAR eventName[255]=L"Event1";							//which event number to change
TCHAR mapToVKEYName[255]=L"mapToVKEY";					//store and read VKEY to map to

DWORD mapToVKEY = 0x0d;

//update driver?
//#include <KBDTools.h>
HINSTANCE h_kbdtools=NULL;
typedef HRESULT (*PFN_SetKeyboardNotify)(BOOL enable);
PFN_SetKeyboardNotify SetKeyboardNotify=NULL;
HRESULT notifyDriver(){
	HRESULT hRes=-99;
	DEBUGMSG(1, (L"notifyDriver...\n"));
	if(h_kbdtools==NULL)
		h_kbdtools=LoadLibrary(L"kbdtools.cpl");
	if(h_kbdtools){
		if(SetKeyboardNotify==NULL){
			SetKeyboardNotify = (PFN_SetKeyboardNotify)GetProcAddress(h_kbdtools, L"SetKeyboardNotify");
		}
		if(SetKeyboardNotify){
			hRes=SetKeyboardNotify(TRUE);
			DEBUGMSG(1, (L"called SetKeyboardNotify\n"));
		}
		else
			DEBUGMSG(1, (L"no SetKeyboardNotify found\n"));
	}
	else{
		DEBUGMSG(1, (L"no kbdtools.cpl found\n"));
	}
	return hRes;
}

void writeReg(){
	OpenCreateKey(regAppKey);
	RegWriteStr(L"eventName", eventName);
	RegWriteDword(mapToVKEYName, &mapToVKEY);
}

void readReg(){
	OpenCreateKey(regAppKey);
	TCHAR tempStr[255];
	//which indexed event to patch
	if(RegReadStr(eventName, tempStr)==0)
		wsprintf(eventName, L"%s", tempStr);
	else
		writeReg();
	DEBUGMSG(1, (L"eventName='%s'\n",eventName));
	DWORD tempDW=0;
	//which VKEY to send
	if(RegReadDword(mapToVKEYName, &tempDW)==0)
		mapToVKEY=tempDW;
	DEBUGMSG(1, (L"mapToVKEY=0x%02x\n",mapToVKEY));
}

void patchEvents(){
	DEBUGMSG(1, (L"patchEvents...\n"));
	//get keyboard table reg location
	OpenCreateKey(regCurrentActiveLayoutKeyPath);
	TCHAR tempStr[MAX_PATH];
	if(RegReadStr(regCurrentActiveLayoutValueName, tempStr)==0){
		wsprintf(regCurrentLayoutPath, L"%s", tempStr);
		DEBUGMSG(1, (L"regCurrentLayoutPath: '%s'\n", regCurrentLayoutPath));//Drivers\HID\ClientDrivers\ITCKeyboard\Layout\CN75AN5-Numeric\0001
	}
	wsprintf(regPathKeyboardEventDelta, L"%s\\Events\\Delta", regCurrentLayoutPath);//Drivers\HID\ClientDrivers\ITCKeyboard\Layout\CN75AN5-Numeric\0001\Events\Delta
	wsprintf(regPathKeyboardEventState, L"%s\\Events\\State", regCurrentLayoutPath);//Drivers\HID\ClientDrivers\ITCKeyboard\Layout\CN75AN5-Numeric\0001\Events\State
	DEBUGMSG(1, (L"Delta location='%s', State location='%s'\n", regPathKeyboardEventDelta, regPathKeyboardEventState));
	BOOL bNeedsReboot=FALSE;
	//write new delata event name?
	OpenKey(regPathKeyboardEventDelta);
	//test if change needed
	if(RegReadStr(eventName, tempStr)==0){
		if(wcscmp(tempStr, NEW_NAME_DELTA)!=0){
			//write new delta event name
			RegWriteStr(eventName, NEW_NAME_DELTA);
			bNeedsReboot=TRUE;
		}
	}else{
		//write new delta event name
		RegWriteStr(eventName, NEW_NAME_DELTA);
		bNeedsReboot=TRUE;
	}
	//write new state event name?
	OpenKey(regPathKeyboardEventState);
	if(RegReadStr(eventName, tempStr)==0){
		if(wcscmp(tempStr, NEW_NAME_STATE)!=0){
			//write new delta event name
			RegWriteStr(eventName, NEW_NAME_STATE);
			bNeedsReboot=TRUE;
		}
	}else{
		//write new delta event name
		RegWriteStr(eventName, NEW_NAME_STATE);
		bNeedsReboot=TRUE;
	}
	DEBUGMSG(1, (L"patchEvents...DONE\n"));
	if(bNeedsReboot){
		HRESULT hRes=notifyDriver();
		if(hRes!=S_OK){
			MessageBox(g_hWnd, L"Please reboot to activate change.", L"Registry changed", MB_ICONEXCLAMATION|MB_OK);
		}
	}
	
}

//event handles
HANDLE h_Delta=NULL;
HANDLE h_State=NULL;
HANDLE h_stopThread=NULL;
BOOL   b_stopThread=FALSE;
//thread stuff
HANDLE h_thread=NULL;
DWORD  dw_threadID=0;
#define KEYEVENTF_KEYDOWN 0

void sendKey(){
	HWND hwnd_current=GetForegroundWindow();
	ShowWindow(hwnd_current, SW_SHOWNORMAL);
	keybd_event((BYTE)mapToVKEY, 0, KEYEVENTF_KEYDOWN | KEYEVENTF_SILENT, 0);
	Sleep(1);
	keybd_event((BYTE)mapToVKEY, 0, KEYEVENTF_KEYUP | KEYEVENTF_SILENT, 0);
}

DWORD myThread(LPVOID parms){
	HANDLE events[3];
	events[0]=h_State; //autoreset event!
	events[1]=h_Delta; //manual reset event
	events[2]=h_stopThread;
	DEBUGMSG(1, (L"myThread START...\n"));
	DWORD dwEvent;
	BOOL lastDelta=FALSE, lasteState;
	do{
		dwEvent=WaitForMultipleObjects(3, events, FALSE, 1000);
		switch(dwEvent){
			case WAIT_OBJECT_0:
				//state event
				DEBUGMSG(1, (L"got STATE event\n"));
				break;
			case WAIT_OBJECT_0+1:
				//delta event
				DEBUGMSG(1, (L"got DELTA event\n"));
				lastDelta=!lastDelta;
				//if(lastDelta)
					sendKey();
				ResetEvent(h_Delta);
				break;
			case WAIT_OBJECT_0+2:
				//stop thread
				DEBUGMSG(1, (L"got stop_thread event\n"));
				b_stopThread=TRUE;
				break;
			case WAIT_TIMEOUT:
				break;
		}
		Sleep(1);
	}while(b_stopThread==FALSE);
	DEBUGMSG(1, (L"myThread STOPPED\n"));
	return 0;
}

void startThread(){
	BOOL manualReset=TRUE;
	BOOL autoReset  =FALSE;
	//create event handles
	if(h_State==NULL)
		h_State = CreateEvent(NULL, autoReset, FALSE, NEW_NAME_STATE);
	if(h_Delta==NULL)
		h_Delta = CreateEvent(NULL, manualReset, FALSE, NEW_NAME_DELTA);
	//create thread stop event
	if(h_stopThread==NULL)
		h_stopThread = CreateEvent(NULL, manualReset, FALSE, L"STOP_THREAD");
	b_stopThread=FALSE;

	h_thread = CreateThread(NULL, 0, myThread, NULL, 0, &dw_threadID);
	DEBUGMSG(1, (L"startThread done: 0x%02x\n", h_thread));
}

void stopThread(){
	if(h_thread){
		b_stopThread=TRUE;
		SetEvent(h_stopThread);
	}
}


#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE			g_hInst;			// current instance
HWND				g_hWndMenuBar;		// menu bar handle

// Forward declarations of functions included in this code module:
ATOM			MyRegisterClass(HINSTANCE, LPTSTR);
BOOL			InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPTSTR    lpCmdLine,
                   int       nCmdShow)
{
	MSG msg;

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	HACCEL hAccelTable;
	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_EVENT2KEYWM));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
ATOM MyRegisterClass(HINSTANCE hInstance, LPTSTR szWindowClass)
{
	WNDCLASS wc;

	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EVENT2KEYWM));
	wc.hCursor       = 0;
	wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = szWindowClass;

	return RegisterClass(&wc);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd;
    TCHAR szTitle[MAX_LOADSTRING];		// title bar text
    TCHAR szWindowClass[MAX_LOADSTRING];	// main window class name

    g_hInst = hInstance; // Store instance handle in our global variable

    // SHInitExtraControls should be called once during your application's initialization to initialize any
    // of the device specific controls such as CAPEDIT and SIPPREF.
    SHInitExtraControls();

    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING); 
    LoadString(hInstance, IDC_EVENT2KEYWM, szWindowClass, MAX_LOADSTRING);

    //If it is already running, then focus on the window, and exit
    hWnd = FindWindow(szWindowClass, szTitle);	
    if (hWnd) 
    {
        // set focus to foremost child window
        // The "| 0x00000001" is used to bring any owned windows to the foreground and
        // activate them.
        SetForegroundWindow((HWND)((ULONG) hWnd | 0x00000001));
        return 0;
    } 

    if (!MyRegisterClass(hInstance, szWindowClass))
    {
    	return FALSE;
    }

    hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

	g_hWnd=hWnd;

    if (!hWnd)
    {
        return FALSE;
    }

    // When the main window is created using CW_USEDEFAULT the height of the menubar (if one
    // is created is not taken into account). So we resize the window after creating it
    // if a menubar is present
    if (g_hWndMenuBar)
    {
        RECT rc;
        RECT rcMenuBar;

        GetWindowRect(hWnd, &rc);
        GetWindowRect(g_hWndMenuBar, &rcMenuBar);
        rc.bottom -= (rcMenuBar.bottom - rcMenuBar.top);
		
        MoveWindow(hWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, FALSE);
    }

    ShowWindow(hWnd, SW_MINIMIZE);// nCmdShow);
    UpdateWindow(hWnd);


    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;

    static SHACTIVATEINFO s_sai;
	
    switch (message) 
    {
        case WM_COMMAND:
            wmId    = LOWORD(wParam); 
            wmEvent = HIWORD(wParam); 
            // Parse the menu selections:
            switch (wmId)
            {
                case IDM_HELP_ABOUT:
                    DialogBox(g_hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, About);
                    break;
                case IDM_OK:
                    SendMessage (hWnd, WM_CLOSE, 0, 0);				
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
        case WM_CREATE:
            SHMENUBARINFO mbi;

            memset(&mbi, 0, sizeof(SHMENUBARINFO));
            mbi.cbSize     = sizeof(SHMENUBARINFO);
            mbi.hwndParent = hWnd;
            mbi.nToolBarId = IDR_MENU;
            mbi.hInstRes   = g_hInst;

            if (!SHCreateMenuBar(&mbi)) 
            {
                g_hWndMenuBar = NULL;
            }
            else
            {
                g_hWndMenuBar = mbi.hwndMB;
            }

            // Initialize the shell activate info structure
            memset(&s_sai, 0, sizeof (s_sai));
            s_sai.cbSize = sizeof (s_sai);

			readReg();
			patchEvents();
			startThread();

            break;
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            
            // TODO: Add any drawing code here...
            
            EndPaint(hWnd, &ps);
            break;
        case WM_DESTROY:
            CommandBar_Destroy(g_hWndMenuBar);
			
			stopThread();
			Sleep(1000);

            PostQuitMessage(0);
            break;

        case WM_ACTIVATE:
            // Notify shell of our activate message
            SHHandleWMActivate(hWnd, wParam, lParam, &s_sai, FALSE);
            break;
        case WM_SETTINGCHANGE:
            SHHandleWMSettingChange(hWnd, wParam, lParam, &s_sai);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
            {
                // Create a Done button and size it.  
                SHINITDLGINFO shidi;
                shidi.dwMask = SHIDIM_FLAGS;
                shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN | SHIDIF_SIZEDLGFULLSCREEN | SHIDIF_EMPTYMENU;
                shidi.hDlg = hDlg;
                SHInitDialog(&shidi);
            }
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK)
            {
                EndDialog(hDlg, LOWORD(wParam));
                return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hDlg, message);
            return TRUE;

    }
    return (INT_PTR)FALSE;
}

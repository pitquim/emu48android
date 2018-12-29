//
// Created by cosnier on 12/11/2018.
//
#include <jni.h>
#include <stdio.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/bitmap.h>

#include "core/pch.h"
#include "core/Emu48.h"
#include "core/io.h"
#include "core/kml.h"
#include "win32-layer.h"

extern void emu48Start();
extern AAssetManager * assetManager;
static jobject viewToUpdate = NULL;
static jobject mainActivity = NULL;
jobject bitmapMainScreen;
AndroidBitmapInfo androidBitmapInfo;
TCHAR szChosenCurrentKml[MAX_PATH];
TCHAR szKmlLog[10240];
TCHAR szKmlTitle[10240];
BOOL settingsPort2en;
BOOL settingsPort2wr;



extern void win32Init();

extern void draw();
extern void buttonDown(int x, int y);
extern void buttonUp(int x, int y);
extern void keyDown(int virtKey);
extern void keyUp(int virtKey);

extern void OnViewCopy();
extern void OnBackupSave();
extern void OnBackupRestore();
extern void OnBackupDelete();


JNIEnv *getJNIEnvironment();

JavaVM *java_machine;
JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    java_machine = vm;
    win32Init();
    return JNI_VERSION_1_6;
}

JNIEnv *getJNIEnvironment() {
    JNIEnv * jniEnv;
    jint ret;
    BOOL needDetach = FALSE;
    ret = (*java_machine)->GetEnv(java_machine, &jniEnv, JNI_VERSION_1_6);
    if (ret == JNI_EDETACHED) {
        // GetEnv: not attached
        ret = (*java_machine)->AttachCurrentThread(java_machine, &jniEnv, NULL);
        if (ret == JNI_OK) {
            needDetach = TRUE;
        }
    }
    return jniEnv;
}

enum CALLBACK_TYPE {
    CALLBACK_TYPE_INVALIDATE = 0,
    CALLBACK_TYPE_WINDOW_RESIZE = 1
};

void mainViewUpdateCallback() {
    mainViewCallback(CALLBACK_TYPE_INVALIDATE, 0, 0, NULL, NULL);
}

void mainViewResizeCallback(int x, int y) {
    mainViewCallback(CALLBACK_TYPE_WINDOW_RESIZE, x, y, NULL, NULL);

    JNIEnv * jniEnv;
    int ret = (*java_machine)->GetEnv(java_machine, &jniEnv, JNI_VERSION_1_6);
    ret = AndroidBitmap_getInfo(jniEnv, bitmapMainScreen, &androidBitmapInfo);
    if (ret < 0) {
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
    }
}

// https://stackoverflow.com/questions/9630134/jni-how-to-callback-from-c-or-c-to-java
int mainViewCallback(int type, int param1, int param2, const TCHAR * param3, const TCHAR * param4) {
    if (viewToUpdate) {
        JNIEnv *jniEnv = getJNIEnvironment();
        jclass viewToUpdateClass = (*jniEnv)->GetObjectClass(jniEnv, viewToUpdate);
        jmethodID midStr = (*jniEnv)->GetMethodID(jniEnv, viewToUpdateClass, "updateCallback", "(IIILjava/lang/String;Ljava/lang/String;)I");
        jstring utfParam3 = (*jniEnv)->NewStringUTF(jniEnv, param3);
        jstring utfParam4 = (*jniEnv)->NewStringUTF(jniEnv, param4);
        int result = (*jniEnv)->CallIntMethod(jniEnv, viewToUpdate, midStr, type, param1, param2, utfParam3, utfParam4);
//      if(needDetach) ret = (*java_machine)->DetachCurrentThread(java_machine);
        return result;
    }
}

// Must be called in the main thread
int openFileFromContentResolver(const TCHAR * url, int writeAccess) {
    JNIEnv *jniEnv = getJNIEnvironment();
    jclass mainActivityClass = (*jniEnv)->GetObjectClass(jniEnv, mainActivity);
    jmethodID midStr = (*jniEnv)->GetMethodID(jniEnv, mainActivityClass, "openFileFromContentResolver", "(Ljava/lang/String;I)I");
    jstring utfUrl = (*jniEnv)->NewStringUTF(jniEnv, url);
    int result = (*jniEnv)->CallIntMethod(jniEnv, mainActivity, midStr, utfUrl, writeAccess);
    return result;
}
int closeFileFromContentResolver(int fd) {
    JNIEnv *jniEnv = getJNIEnvironment();
    jclass mainActivityClass = (*jniEnv)->GetObjectClass(jniEnv, mainActivity);
    jmethodID midStr = (*jniEnv)->GetMethodID(jniEnv, mainActivityClass, "closeFileFromContentResolver", "(I)I");
    int result = (*jniEnv)->CallIntMethod(jniEnv, mainActivity, midStr, fd);
    return result;
}

int showAlert(const TCHAR * messageText, int flags) {
    JNIEnv *jniEnv = getJNIEnvironment();
    jclass mainActivityClass = (*jniEnv)->GetObjectClass(jniEnv, mainActivity);
    jmethodID midStr = (*jniEnv)->GetMethodID(jniEnv, mainActivityClass, "showAlert", "(Ljava/lang/String;)V");
    jstring messageUTF = (*jniEnv)->NewStringUTF(jniEnv, messageText);
    (*jniEnv)->CallVoidMethod(jniEnv, mainActivity, midStr, messageUTF);
    return IDOK;
}

void clipboardCopyText(const TCHAR * text) {
    JNIEnv *jniEnv = getJNIEnvironment();
    jclass mainActivityClass = (*jniEnv)->GetObjectClass(jniEnv, mainActivity);
    jmethodID midStr = (*jniEnv)->GetMethodID(jniEnv, mainActivityClass, "clipboardCopyText", "(Ljava/lang/String;)V");
    jstring messageUTF = (*jniEnv)->NewStringUTF(jniEnv, text);
    (*jniEnv)->CallVoidMethod(jniEnv, mainActivity, midStr, messageUTF);
}
const TCHAR * clipboardPasteText() {
    JNIEnv *jniEnv = getJNIEnvironment();
    jclass mainActivityClass = (*jniEnv)->GetObjectClass(jniEnv, mainActivity);
    jmethodID midStr = (*jniEnv)->GetMethodID(jniEnv, mainActivityClass, "clipboardPasteText", "()Ljava/lang/String;");
    jobject result = (*jniEnv)->CallObjectMethod(jniEnv, mainActivity, midStr);
    if(result) {
        const char *strReturn = (*jniEnv)->GetStringUTFChars(jniEnv, result, 0);
        size_t length = _tcslen(strReturn);
        TCHAR * pasteText = GlobalAlloc(0, length + 2);
        _tcscpy(pasteText, strReturn);
        (*jniEnv)->ReleaseStringUTFChars(jniEnv, result, strReturn);
        return pasteText;
    }
    return NULL;
}

JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_start(JNIEnv *env, jobject thisz, jobject assetMgr, jobject bitmapMainScreen0, jobject activity, jobject view) {

    szChosenCurrentKml[0] = '\0';

    bitmapMainScreen = (*env)->NewGlobalRef(env, bitmapMainScreen0);
    mainActivity = (*env)->NewGlobalRef(env, activity);
    viewToUpdate = (*env)->NewGlobalRef(env, view);


    int ret = AndroidBitmap_getInfo(env, bitmapMainScreen, &androidBitmapInfo);
    if (ret < 0) {
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
    }

    assetManager = AAssetManager_fromJava(env, assetMgr);





    DWORD dwThreadId;

    // read emulator settings
    GetCurrentDirectory(ARRAYSIZEOF(szCurrentDirectory),szCurrentDirectory);
    ReadSettings();

    _tcscpy(szCurrentDirectory, "");
    _tcscpy(szEmuDirectory, "assets/calculators/");
    _tcscpy(szRomDirectory, "assets/calculators/");
    _tcscpy(szPort2Filename, "");

    hWindowDC = CreateCompatibleDC(NULL);

    // initialization
    LARGE_INTEGER    lAppStart2;					// high performance counter value at Appl. start

    QueryPerformanceFrequency(&lFreq);		// init high resolution counter
    QueryPerformanceCounter(&lAppStart);
    Sleep(1000);
    QueryPerformanceCounter(&lAppStart2);

    szCurrentKml[0] = 0;					// no KML file selected
    SetSpeed(bRealSpeed);					// set speed
	//MruInit(4);								// init MRU entries

    // create auto event handle
    hEventShutdn = CreateEvent(NULL,FALSE,FALSE,NULL);
    if (hEventShutdn == NULL)
    {
        AbortMessage(_T("Event creation failed."));
//		DestroyWindow(hWnd);
        return;
    }

    nState     = SM_RUN;					// init state must be <> nNextState
    nNextState = SM_INVALID;				// go into invalid state
    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&WorkerThread, NULL, CREATE_SUSPENDED, &dwThreadId);
    if (hThread == NULL)
    {
        CloseHandle(hEventShutdn);			// close event handle
        AbortMessage(_T("Thread creation failed."));
//		DestroyWindow(hWnd);
        return;
    }

    ResumeThread(hThread);					// start thread
    while (nState!=nNextState) Sleep(0);	// wait for thread initialized
}

JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_stop(JNIEnv *env, jobject thisz) {

    if (viewToUpdate) {
        (*env)->DeleteGlobalRef(env, viewToUpdate);
        viewToUpdate = NULL;
    }
    if(bitmapMainScreen) {
        (*env)->DeleteGlobalRef(env, bitmapMainScreen);
        bitmapMainScreen = NULL;
    }
}


//JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_resize(JNIEnv *env, jobject thisz, jint width, jint height) {
//
//}

JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_draw(JNIEnv *env, jobject thisz) {
    draw();
}
JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_buttonDown(JNIEnv *env, jobject thisz, jint x, jint y) {
    buttonDown(x, y);
}
JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_buttonUp(JNIEnv *env, jobject thisz, jint x, jint y) {
    buttonUp(x, y);
}
JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_keyDown(JNIEnv *env, jobject thisz, jint virtKey) {
    keyDown(virtKey);
}
JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_keyUp(JNIEnv *env, jobject thisz, jint virtKey) {
    keyUp(virtKey);
}



JNIEXPORT jboolean JNICALL Java_com_regis_cosnier_emu48_NativeLib_isDocumentAvailable(JNIEnv *env, jobject thisz) {
    return bDocumentAvail ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jstring JNICALL Java_com_regis_cosnier_emu48_NativeLib_getCurrentFilename(JNIEnv *env, jobject thisz) {
    jstring result = (*env)->NewStringUTF(env, szCurrentFilename);
    return result;
}
//JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_setCurrentFilename(JNIEnv *env, jobject thisz, jstring newFilename) {
//    const char *newFilenameUTF8 = (*env)->GetStringUTFChars(env, newFilename , NULL) ;
//    _tcscpy(szBufferFilename, newFilenameUTF8);
//    (*env)->ReleaseStringUTFChars(env, newFilename, newFilenameUTF8);
//}

JNIEXPORT jint JNICALL Java_com_regis_cosnier_emu48_NativeLib_getCurrentModel(JNIEnv *env, jobject thisz) {
    return cCurrentRomType;
}

JNIEXPORT jboolean JNICALL Java_com_regis_cosnier_emu48_NativeLib_isBackup(JNIEnv *env, jobject thisz) {
    return bBackup ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL Java_com_regis_cosnier_emu48_NativeLib_getKMLLog(JNIEnv *env, jobject thisz) {
    jstring result = (*env)->NewStringUTF(env, szKmlLog);
    return result;
}

JNIEXPORT jstring JNICALL Java_com_regis_cosnier_emu48_NativeLib_getKMLTitle(JNIEnv *env, jobject thisz) {
    jstring result = (*env)->NewStringUTF(env, szKmlTitle);
    return result;
}

JNIEXPORT jint JNICALL Java_com_regis_cosnier_emu48_NativeLib_onFileNew(JNIEnv *env, jobject thisz, jstring kmlFilename) {
    //OnFileNew();
    if (bDocumentAvail)
    {
        SwitchToState(SM_INVALID);
        if(bAutoSave) {
            SaveDocument();
        }
    }

    const char *filenameUTF8 = (*env)->GetStringUTFChars(env, kmlFilename , NULL) ;
    _tcscpy(szChosenCurrentKml, filenameUTF8);
    (*env)->ReleaseStringUTFChars(env, kmlFilename, filenameUTF8);

    BOOL result = NewDocument();

    mainViewResizeCallback(nBackgroundW, nBackgroundH);
    draw();
    if (bStartupBackup) SaveBackup();        // make a RAM backup at startup

    if (pbyRom) SwitchToState(SM_RUN);

    return result;
}
JNIEXPORT jint JNICALL Java_com_regis_cosnier_emu48_NativeLib_onFileOpen(JNIEnv *env, jobject thisz, jstring stateFilename) {
    //OnFileOpen();
    if (bDocumentAvail)
    {
        SwitchToState(SM_INVALID);
        if(bAutoSave) {
            SaveDocument();
        }
    }
    const char *stateFilenameUTF8 = (*env)->GetStringUTFChars(env, stateFilename , NULL) ;
    _tcscpy(szBufferFilename, stateFilenameUTF8);
    BOOL result = OpenDocument(szBufferFilename);
    if (result)
        MruAdd(szBufferFilename);
    mainViewResizeCallback(nBackgroundW, nBackgroundH);
    if (pbyRom) SwitchToState(SM_RUN);
    draw();
    (*env)->ReleaseStringUTFChars(env, stateFilename, stateFilenameUTF8);
    return result;
}
JNIEXPORT jint JNICALL Java_com_regis_cosnier_emu48_NativeLib_onFileSave(JNIEnv *env, jobject thisz) {
    // szBufferFilename must be set before calling that!!!
    //OnFileSave();
    BOOL result = FALSE;
    if (bDocumentAvail)
    {
        SwitchToState(SM_INVALID);
        result = SaveDocument();
        SwitchToState(SM_RUN);
    }
    return result;
}
JNIEXPORT jint JNICALL Java_com_regis_cosnier_emu48_NativeLib_onFileSaveAs(JNIEnv *env, jobject thisz, jstring newStateFilename) {
    const char *newStateFilenameUTF8 = (*env)->GetStringUTFChars(env, newStateFilename , NULL) ;

    BOOL result = FALSE;
    if (bDocumentAvail)
    {
        SwitchToState(SM_INVALID);
        _tcscpy(szBufferFilename, newStateFilenameUTF8);
        result = SaveDocumentAs(szBufferFilename);
        if (result)
            MruAdd(szCurrentFilename);
        else {
            //TODO ERROR !!!!!!!!!
        }
        SwitchToState(SM_RUN);
    }

    (*env)->ReleaseStringUTFChars(env, newStateFilename, newStateFilenameUTF8);
    return result;
}

JNIEXPORT jint JNICALL Java_com_regis_cosnier_emu48_NativeLib_onFileClose(JNIEnv *env, jobject thisz) {
    if (bDocumentAvail)
    {
        SwitchToState(SM_INVALID);
        if(bAutoSave)
            SaveDocument();
        ResetDocument();
        SetWindowTitle(NULL);
        szKmlTitle[0] = '\0';
        mainViewResizeCallback(nBackgroundW, nBackgroundH);
        draw();
        return TRUE;
    }
    return FALSE;
}

JNIEXPORT jint JNICALL Java_com_regis_cosnier_emu48_NativeLib_onObjectLoad(JNIEnv *env, jobject thisz, jstring filename) {
    const char *filenameUTF8 = (*env)->GetStringUTFChars(env, filename , NULL) ;

    SuspendDebugger();						// suspend debugger
    bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

    // calculator off, turn on
    if (!(Chipset.IORam[BITOFFSET]&DON))
    {
        KeyboardEvent(TRUE,0,0x8000);
        Sleep(dwWakeupDelay);
        KeyboardEvent(FALSE,0,0x8000);

        // wait for sleep mode
        while (Chipset.Shutdn == FALSE) Sleep(0);
    }

    if (nState != SM_RUN)
    {
        InfoMessage(_T("The emulator must be running to load an object."));
        goto cancel;
    }

    if (WaitForSleepState())				// wait for cpu SHUTDN then sleep state
    {
        InfoMessage(_T("The emulator is busy."));
        goto cancel;
    }

    _ASSERT(nState == SM_SLEEP);

//    if (bLoadObjectWarning)
//    {
//        UINT uReply = YesNoCancelMessage(
//                _T("Warning: Trying to load an object while the emulator is busy\n")
//                _T("will certainly result in a memory lost. Before loading an object\n")
//                _T("you should be sure that the calculator is in idle state.\n")
//                _T("Do you want to see this warning next time you try to load an object?"),0);
//        switch (uReply)
//        {
//            case IDYES:
//                break;
//            case IDNO:
//                bLoadObjectWarning = FALSE;
//                break;
//            case IDCANCEL:
//                SwitchToState(SM_RUN);
//                goto cancel;
//        }
//    }

//    if (!GetLoadObjectFilename(_T(HP_FILTER),_T("HP")))
//    {
//        SwitchToState(SM_RUN);
//        goto cancel;
//    }

    if (!LoadObject(filenameUTF8))
    {
        SwitchToState(SM_RUN);
        goto cancel;
    }

    SwitchToState(SM_RUN);					// run state
    while (nState!=nNextState) Sleep(0);
    _ASSERT(nState == SM_RUN);
    KeyboardEvent(TRUE,0,0x8000);
    Sleep(dwWakeupDelay);
    KeyboardEvent(FALSE,0,0x8000);
    while (Chipset.Shutdn == FALSE) Sleep(0);

    cancel:
    bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
    ResumeDebugger();

    (*env)->ReleaseStringUTFChars(env, filename, filenameUTF8);

    return TRUE;
}

JNIEXPORT jint JNICALL Java_com_regis_cosnier_emu48_NativeLib_onObjectSave(JNIEnv *env, jobject thisz, jstring filename) {
    const char *filenameUTF8 = (*env)->GetStringUTFChars(env, filename , NULL) ;
    //OnObjectSave();

    if (nState != SM_RUN)
    {
        InfoMessage(_T("The emulator must be running to save an object."));
        return 0;
    }

    if (WaitForSleepState())				// wait for cpu SHUTDN then sleep state
    {
        InfoMessage(_T("The emulator is busy."));
        return 0;
    }

    _ASSERT(nState == SM_SLEEP);

//    if (GetSaveObjectFilename(_T(HP_FILTER),_T("HP")))
//    {
        SaveObject(filenameUTF8);
//    }

    SwitchToState(SM_RUN);

    (*env)->ReleaseStringUTFChars(env, filename, filenameUTF8);

    return TRUE;
}

JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_onViewCopy(JNIEnv *env, jobject thisz, jobject bitmapScreen) {

    //jobject bitmapScreen = (*env)->NewGlobalRef(env, bitmapScreen0);

    AndroidBitmapInfo bitmapScreenInfo;
    int ret = AndroidBitmap_getInfo(env, bitmapScreen, &bitmapScreenInfo);
    if (ret < 0) {
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
        return;
    }

    void * pixelsDestination;
    if ((ret = AndroidBitmap_lockPixels(env, bitmapScreen, &pixelsDestination)) < 0) {
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
    }

//    void * pixelsSource = hBitmap->bitmapBits;
//
//    int sourceWidth = hBitmap->bitmapInfoHeader->biWidth;
//    int sourceHeight = abs(hBitmap->bitmapInfoHeader->biHeight);
//    int destinationWidth = bitmapScreenInfo.width;
//    int destinationHeight = bitmapScreenInfo.height;
//
//    //https://softwareengineering.stackexchange.com/questions/148123/what-is-the-algorithm-to-copy-a-region-of-one-bitmap-into-a-region-in-another
//    float src_dx = (float)wSrc / (float)wDest;
//    float src_dy = (float)hSrc / (float)hDest;
//    float src_maxx = xSrc + wSrc;
//    float src_maxy = ySrc + hSrc;
//    float dst_maxx = xDest + wDest;
//    float dst_maxy = yDest + hDest;
//    float src_cury = ySrc;
//
//    int sourceBytes = (hBitmap->bitmapInfoHeader->biBitCount >> 3);
//    float sourceStride = sourceWidth * sourceBytes;
//    sourceStride = (float)(4 * ((sourceWidth * hBitmap->bitmapInfoHeader->biBitCount + 31) / 32));
//    float destinationStride = bitmapScreenInfo.stride; // Destination always 4 bytes RGBA
//    //LOGD("StretchBlt(%08x, x:%d, y:%d, w:%d, h:%d, %08x, x:%d, y:%d, w:%d, h:%d) -> sourceBytes: %d", hdcDest->hdcCompatible, xDest, yDest, wDest, hDest, hdcSrc, xSrc, ySrc, wSrc, hSrc, sourceBytes);
//
//    PALETTEENTRY * palPalEntry = hdcSrc->selectedPalette && hdcSrc->selectedPalette->paletteLog && hdcSrc->selectedPalette->paletteLog->palPalEntry ?
//                                 hdcSrc->selectedPalette->paletteLog->palPalEntry : NULL;
//
//    for (float y = yDest; y < dst_maxy; y++)
//    {
//        float src_curx = xSrc;
//        for (float x = xDest; x < dst_maxx; x++)
//        {
//            // Point sampling - you can also impl as bilinear or other
//            //dst.bmp[x,y] = src.bmp[src_curx, src_cury];
//
//            BYTE * destinationPixel = pixelsDestination + (int)(4.0 * x + destinationStride * y);
//            BYTE * sourcePixel = pixelsSource + (int)(sourceBytes * (int)src_curx) + (int)(sourceStride * (int)src_cury);
//
//            // -> ARGB_8888
//            switch (sourceBytes) {
//                case 1:
//                    if(palPalEntry) {
//                        BYTE colorIndex = sourcePixel[0];
//                        destinationPixel[0] = palPalEntry[colorIndex].peBlue;
//                        destinationPixel[1] = palPalEntry[colorIndex].peGreen;
//                        destinationPixel[2] = palPalEntry[colorIndex].peRed;
//                        destinationPixel[3] = 255;
//                    } else {
//                        destinationPixel[0] = sourcePixel[0];
//                        destinationPixel[1] = sourcePixel[0];
//                        destinationPixel[2] = sourcePixel[0];
//                        destinationPixel[3] = 255;
//                    }
//                    break;
//                case 3:
//                    destinationPixel[0] = sourcePixel[2];
//                    destinationPixel[1] = sourcePixel[1];
//                    destinationPixel[2] = sourcePixel[0];
//                    destinationPixel[3] = 255;
//                    break;
//                case 4:
//                    memcpy(destinationPixel, sourcePixel, sourceBytes);
//                    break;
//                default:
//                    break;
//            }
//
//            src_curx += src_dx;
//        }
//
//        src_cury += src_dy;
//    }


    // DIB bitmap
    #define WIDTHBYTES(bits) (((bits) + 31) / 32 * 4)
    #define PALVERSION       0x300

    BITMAP bm;
    LPBITMAPINFOHEADER lpbi;
    PLOGPALETTE ppal;
    HBITMAP hBmp;
    HDC hBmpDC;
    HANDLE hClipObj;
    WORD wBits;
    DWORD dwLen, dwSizeImage;

    _ASSERT(nLcdZoom >= 1 && nLcdZoom <= 4);
    hBmp = CreateCompatibleBitmap(hLcdDC,131*nLcdZoom*nGdiXZoom,SCREENHEIGHT*nLcdZoom*nGdiYZoom);   // CdB for HP: add apples display stuff
    hBmpDC = CreateCompatibleDC(hLcdDC);
    hBmp = (HBITMAP) SelectObject(hBmpDC,hBmp);
    EnterCriticalSection(&csGDILock); // solving NT GDI problems
    {
        UINT nLines = MAINSCREENHEIGHT;

        // copy header display area
        StretchBlt(hBmpDC, 0, 0,
                   131*nLcdZoom*nGdiXZoom, Chipset.d0size*nLcdZoom*nGdiYZoom,
                   hLcdDC, Chipset.d0offset, 0,
                   131, Chipset.d0size, SRCCOPY);
        // copy main display area
        StretchBlt(hBmpDC, 0, Chipset.d0size*nLcdZoom*nGdiYZoom,
                   131*nLcdZoom*nGdiXZoom, nLines*nLcdZoom*nGdiYZoom,
                   hLcdDC, Chipset.boffset, Chipset.d0size,
                   131, nLines, SRCCOPY);
        // copy menu display area
        StretchBlt(hBmpDC, 0, (nLines+Chipset.d0size)*nLcdZoom*nGdiYZoom,
                   131*nLcdZoom*nGdiXZoom, MENUHEIGHT*nLcdZoom*nGdiYZoom,
                   hLcdDC, 0, (nLines+Chipset.d0size),
                   131, MENUHEIGHT, SRCCOPY);
        GdiFlush();
    }
    LeaveCriticalSection(&csGDILock);
    hBmp = (HBITMAP) SelectObject(hBmpDC,hBmp);

    // fill BITMAP structure for size information
    GetObject(hBmp, sizeof(bm), &bm);

    wBits = bm.bmPlanes * bm.bmBitsPixel;
    // make sure bits per pixel is valid
    if (wBits <= 1)
        wBits = 1;
    else if (wBits <= 4)
        wBits = 4;
    else if (wBits <= 8)
        wBits = 8;
    else // if greater than 8-bit, force to 24-bit
        wBits = 24;

    dwSizeImage = WIDTHBYTES((DWORD)bm.bmWidth * wBits) * bm.bmHeight;

    // calculate memory size to store CF_DIB data
    dwLen = sizeof(BITMAPINFOHEADER) + dwSizeImage;
    if (wBits != 24)				// a 24 bitcount DIB has no color table
    {
        // add size for color table
        dwLen += (DWORD) (1 << wBits) * sizeof(RGBQUAD);
    }





    size_t strideSource = (size_t)(4 * ((hBmp->bitmapInfoHeader->biWidth * hBmp->bitmapInfoHeader->biBitCount + 31) / 32));
    size_t strideDestination = bitmapScreenInfo.stride;
    VOID * bitmapBitsSource = (VOID *)hBmp->bitmapBits;
    VOID * bitmapBitsDestination = pixelsDestination;
    for(int y = 0; y < hBmp->bitmapInfoHeader->biHeight; y++) {
        memcpy(bitmapBitsDestination, bitmapBitsSource, strideSource);
        bitmapBitsSource += strideSource;
        bitmapBitsDestination += strideDestination;
    }


//    // memory allocation for clipboard data
//    if ((hClipObj = GlobalAlloc(GMEM_MOVEABLE, dwLen)) != NULL)
//    {
//        lpbi = (LPBITMAPINFOHEADER ) GlobalLock(hClipObj);
//        // initialize BITMAPINFOHEADER
//        lpbi->biSize = sizeof(BITMAPINFOHEADER);
//        lpbi->biWidth = bm.bmWidth;
//        lpbi->biHeight = bm.bmHeight;
//        lpbi->biPlanes = 1;
//        lpbi->biBitCount = wBits;
//        lpbi->biCompression = BI_RGB;
//        lpbi->biSizeImage = dwSizeImage;
//        lpbi->biXPelsPerMeter = 0;
//        lpbi->biYPelsPerMeter = 0;
//        lpbi->biClrUsed = 0;
//        lpbi->biClrImportant = 0;
//        // get bitmap color table and bitmap data
//        GetDIBits(hBmpDC, hBmp, 0, lpbi->biHeight, (LPBYTE)lpbi + dwLen - dwSizeImage,
//                  (LPBITMAPINFO)lpbi, DIB_RGB_COLORS);
//        GlobalUnlock(hClipObj);
//        SetClipboardData(CF_DIB, hClipObj);
//
//        // get number of entries in the logical palette
//        GetObject(hPalette,sizeof(WORD),&wBits);
//
//        // memory allocation for temporary palette data
//        if ((ppal = (PLOGPALETTE) calloc(sizeof(LOGPALETTE) + wBits * sizeof(PALETTEENTRY),1)) != NULL)
//        {
//            ppal->palVersion    = PALVERSION;
//            ppal->palNumEntries = wBits;
//            GetPaletteEntries(hPalette, 0, wBits, ppal->palPalEntry);
//            SetClipboardData(CF_PALETTE, CreatePalette(ppal));
//            free(ppal);
//        }
//    }
    DeleteDC(hBmpDC);
    DeleteObject(hBmp);
    #undef WIDTHBYTES
    #undef PALVERSION


    AndroidBitmap_unlockPixels(env, bitmapScreen);
}

JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_onStackCopy(JNIEnv *env, jobject thisz) {
    OnStackCopy();
}

JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_onStackPaste(JNIEnv *env, jobject thisz) {
    //TODO Memory leak -> No GlobalFree of the paste data!!!!
    OnStackPaste();
}

JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_onViewReset(JNIEnv *env, jobject thisz) {
    //OnViewReset();
    if (nState != SM_RUN)
        return;
    SwitchToState(SM_SLEEP);
    CpuReset();							// register setting after Cpu Reset
    SwitchToState(SM_RUN);
}

JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_onViewScript(JNIEnv *env, jobject thisz, jstring kmlFilename) {

    TCHAR szKmlFile[MAX_PATH];
    BOOL  bKMLChanged,bSucc;
    BYTE cType = cCurrentRomType;
    SwitchToState(SM_INVALID);

    const char *filenameUTF8 = (*env)->GetStringUTFChars(env, kmlFilename , NULL) ;
    _tcscpy(szCurrentKml, filenameUTF8);
    (*env)->ReleaseStringUTFChars(env, kmlFilename, filenameUTF8);

    // make a copy of the current KML script file name
    _ASSERT(sizeof(szKmlFile) == sizeof(szCurrentKml));
    lstrcpyn(szKmlFile,szCurrentKml,ARRAYSIZEOF(szKmlFile));

    bKMLChanged = FALSE;					// KML script not changed
    bSucc = TRUE;							// KML script successful loaded

//    do
//    {
//        if (!DisplayChooseKml(cType))		// quit with Cancel
//        {
//            if (!bKMLChanged)				// KML script not changed
//                break;						// exit loop with current loaded KML script
//
//            // restore KML script file name
//            lstrcpyn(szCurrentKml,szKmlFile,ARRAYSIZEOF(szCurrentKml));
//
//            // try to restore old KML script
//            if ((bSucc = InitKML(szCurrentKml,FALSE)))
//                break;						// exit loop with success
//
//            // restoring failed, save document
//            if (IDCANCEL != SaveChanges(bAutoSave))
//                break;						// exit loop with no success
//
//            _ASSERT(bSucc == FALSE);		// for continuing loop
//        }
//        else								// quit with Ok
//        {
            bKMLChanged = TRUE;				// KML script changed
            bSucc = InitKML(szCurrentKml,FALSE);
//        }
//    }
//    while (!bSucc);							// retry if KML script is invalid

    if (bSucc)
    {
        if (Chipset.wRomCrc != wRomCrc)		// ROM changed
        {
            CpuReset();
            Chipset.Shutdn = FALSE;			// automatic restart

            Chipset.wRomCrc = wRomCrc;		// update current ROM fingerprint
        }
        if (pbyRom) SwitchToState(SM_RUN);	// continue emulation
    }
    else
    {
        ResetDocument();					// close document
        SetWindowTitle(NULL);
    }
    mainViewResizeCallback(nBackgroundW, nBackgroundH);
    draw();
}

JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_onBackupSave(JNIEnv *env, jobject thisz) {
    OnBackupSave();
}

JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_onBackupRestore(JNIEnv *env, jobject thisz) {
    OnBackupRestore();
}

JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_onBackupDelete(JNIEnv *env, jobject thisz) {
    OnBackupDelete();
}

JNIEXPORT void JNICALL Java_com_regis_cosnier_emu48_NativeLib_setConfiguration(JNIEnv *env, jobject thisz, jstring key, jint isDynamic, jint intValue1, jint intValue2, jstring stringValue) {
    const char *configKey = (*env)->GetStringUTFChars(env, key, NULL) ;
    const char *configStringValue = stringValue ? (*env)->GetStringUTFChars(env, stringValue, NULL) : NULL;

    bAutoSave = FALSE;
    bAutoSaveOnExit = FALSE;
    bLoadObjectWarning = FALSE;
    bAlwaysDisplayLog = TRUE;

    if(_tcscmp(_T("settings_realspeed"), configKey) == 0) {
        bRealSpeed = intValue1;
        if(isDynamic)
            SetSpeed(bRealSpeed);			// set speed
    } else if(_tcscmp(_T("settings_grayscale"), configKey) == 0) {
        // LCD grayscale checkbox has been changed
        if (bGrayscale != (BOOL)intValue1) {
            UINT nOldState = SwitchToState(SM_INVALID);
            SetLcdMode(!bGrayscale);	// set new display mode
            SwitchToState(nOldState);
        }
//    } else if(_tcscmp(_T("settings_alwaysdisplog"), configKey) == 0) {
//        bAlwaysDisplayLog = intValue1;
    } else if(_tcscmp(_T("settings_port1"), configKey) == 0) {
        BOOL settingsPort1en = intValue1;
        BOOL settingsPort1wr = intValue2;
        // port1
        if (Chipset.Port1Size && (cCurrentRomType!='X' || cCurrentRomType!='2' || cCurrentRomType!='Q'))   // CdB for HP: add apples
        {
            UINT nOldState = SwitchToState(SM_SLEEP);
            // save old card status
            BYTE byCardsStatus = Chipset.cards_status;

            // port1 disabled?
            Chipset.cards_status &= ~(PORT1_PRESENT | PORT1_WRITE);
            if (settingsPort1en)
            {
                Chipset.cards_status |= PORT1_PRESENT;
                if (settingsPort1wr)
                    Chipset.cards_status |= PORT1_WRITE;
            }

            // changed card status in slot1?
            if (   ((byCardsStatus ^ Chipset.cards_status) & (PORT1_PRESENT | PORT1_WRITE)) != 0
                   && (Chipset.IORam[CARDCTL] & ECDT) != 0 && (Chipset.IORam[TIMER2_CTRL] & RUN) != 0
                    )
            {
                Chipset.HST |= MP;		// set Module Pulled
                IOBit(SRQ2,NINT,FALSE);	// set NINT to low
                Chipset.SoftInt = TRUE;	// set interrupt
                bInterrupt = TRUE;
            }
            SwitchToState(nOldState);
        }
    } else if(_tcscmp(_T("settings_port2"), configKey) == 0) {
        settingsPort2en = (BOOL)intValue1;
        settingsPort2wr = (BOOL)intValue2;
        const char * settingsPort2load = settingsPort2en ? configStringValue : NULL;

        LPCTSTR szActPort2Filename = _T("");
        BOOL bPort2CfgChange = FALSE;
        BOOL bPort2AttChange = FALSE;

        // HP48SX/GX port2 change settings detection
        if (cCurrentRomType=='S' || cCurrentRomType=='G' || cCurrentRomType==0)
        {
            if(settingsPort2en && settingsPort2load) {
                if(_tcscmp(szPort2Filename, settingsPort2load) != 0) {
                    _tcscpy(szPort2Filename, settingsPort2load);
                    bPort2CfgChange = TRUE;    // slot2 configuration changed
                }
                szActPort2Filename = szPort2Filename;

                // R/W port
                if (*szActPort2Filename != 0 && (BOOL) settingsPort2wr != bPort2Writeable) {
                    bPort2AttChange = TRUE;    // slot2 file R/W attribute changed
                    bPort2CfgChange = TRUE;    // slot2 configuration changed
                }
            } else {
                if(szPort2Filename[0] != '\0') {
                    bPort2CfgChange = TRUE;    // slot2 configuration changed
                    szPort2Filename[0] = '\0';
                }
            }
        }

        if (bPort2CfgChange)			// slot2 configuration changed
        {
            UINT nOldState = SwitchToState(SM_INVALID);

            UnmapPort2();				// unmap port2

            if (cCurrentRomType)		// ROM defined
            {
                MapPort2(szActPort2Filename);

                // port2 changed and card detection enabled
                if (   (bPort2AttChange || Chipset.wPort2Crc != wPort2Crc)
                       && (Chipset.IORam[CARDCTL] & ECDT) != 0 && (Chipset.IORam[TIMER2_CTRL] & RUN) != 0
                        )
                {
                    Chipset.HST |= MP;		// set Module Pulled
                    IOBit(SRQ2,NINT,FALSE);	// set NINT to low
                    Chipset.SoftInt = TRUE;	// set interrupt
                    bInterrupt = TRUE;
                }
                // save fingerprint of port2
                Chipset.wPort2Crc = wPort2Crc;
            }
            SwitchToState(nOldState);
        }
    }

    if(configKey)
        (*env)->ReleaseStringUTFChars(env, key, configKey);
    if(configStringValue)
        (*env)->ReleaseStringUTFChars(env, stringValue, configStringValue);
}

JNIEXPORT jboolean JNICALL Java_com_regis_cosnier_emu48_NativeLib_isPortExtensionPossible(JNIEnv *env, jobject thisz) {
    return (cCurrentRomType=='S' || cCurrentRomType=='G' || cCurrentRomType==0 ? JNI_TRUE : JNI_FALSE);
}

JNIEXPORT jint JNICALL Java_com_regis_cosnier_emu48_NativeLib_getState(JNIEnv *env, jobject thisz) {
    return nState;
}

JNIEXPORT jint JNICALL Java_com_regis_cosnier_emu48_NativeLib_getScreenWidth(JNIEnv *env, jobject thisz) {
    return 131*nLcdZoom*nGdiXZoom;
}
JNIEXPORT jint JNICALL Java_com_regis_cosnier_emu48_NativeLib_getScreenHeight(JNIEnv *env, jobject thisz) {
    return SCREENHEIGHT*nLcdZoom*nGdiYZoom;
}


//p Read5(0x7050E)
//   -> $1 = 461076
//p Read5(0x70914)
//   -> 31 ?
/* $Id: mathlink.c,v 1.37 2011/11/09 22:43:52 stevew Exp $ */
#define MLINTERFACE 3
#include "Python.h"
#include "mathlink.h"

#define PYTHONLINKVERSION "0.0.4"      /* Version string available as the variable pythonlinkversion */
#define ERRMSGLEN 128                  /* Maximum length of error message buffer */
#define FUNCTIONHANDLERARGS 2          /* Number of arguments allocated for function handlers */
#define DIALOGERROR -0x0FFFFFFF        /* Error number returned from dialog functions */
#define REQUESTBUFFERSIZE 256          /* Size of buffer used in request, requestargv */
#define ARGVSIZE 20                    /* Size of argv used in requestargv */
#define MLDEVICE_BUFF_LEN 256          /* Size of char buffer used in deviceinformation... */
#define FEATURESTRINGSIZE 256          /* Size of char buffer used in featurestring... */

/* #define USE_PYTHON_MEMORY_ALLOCATOR 1 */  /* Enable this define in order to have the module use Python's memory allocator. */

/************************* module level variable definitions ********************/

char *commandlinelist[] =
	{"env", "name","protocol","mode","host","options","launch","create","connect","authentication","device",NULL};

staticforward PyTypeObject mathlink_EnvType;
staticforward PyTypeObject mathlink_LinkType;
staticforward PyTypeObject mathlink_MarkType;

static PyObject *TokenDictionary;

typedef struct{
	PyObject *func;
	long arg[FUNCTIONHANDLERARGS];
} mathlink_FunctionHandler;

typedef struct{
	PyObject_HEAD
	MLENV env;

	/* The various dialog functions */
	PyObject *ConfirmFunction;
	PyObject *AlertFunction;
	PyObject *RequestFunction;
	PyObject *RequestArgvFunction;
	PyObject *RequestToInteractFunction;

	/* Default yield function */
	PyObject *DefaultYieldFunction;
} mathlink_Env;

typedef struct{
	PyObject_HEAD  
	mathlink_Env * PyEnv;                        /* Python version of the MLENV object */
	MLINK lp;                                    /* link pointer */
	int autoclear;                               /* By default mathlink.link objects will automatically run MLClearError */
	int connected;                               /* Indicates whether a link is currently connected... */
	mathlink_FunctionHandler yieldfunction;      /* We want to be able to install Python yield functions... */
	mathlink_FunctionHandler messagehandler;     /* We want to be able to install Python Message handler functions... */
} mathlink_Link;

typedef struct{
	PyObject_HEAD
	MLINKMark mp;
	PyObject *link;                              /* Reference to the link object that created the mark... */
} mathlink_Mark;

/* Here are the new Exception objects... */
static PyObject *mathlinkError;
static PyObject *mathlinkReadyParallelError;

/* Size of argv and request buffers... */
long margvsize = ARGVSIZE;
long requestbuffersize = REQUESTBUFFERSIZE;

typedef struct messagecode{
	char *mes;
	unsigned long code;
} message_code;


static message_code mathlink_EncodingCodes[] = {
	{"MLASCII_ENC",		MLASCII_ENC},
	{"MLBYTES_ENC",		MLBYTES_ENC},
	{"MLUCS2_ENC",		MLUCS2_ENC},
	{"MLOLD_ENC",		MLOLD_ENC},
	{"MLUTF8_ENC",		MLUTF8_ENC},
	{"MLUTF16_ENC",		MLUTF16_ENC},
	{"MLUTF32_ENC",		MLUTF32_ENC},
	{(char *)0}
};


static message_code mathlink_ErrorCodes[] = {
	{"MLEUNKNOWN",         MLEUNKNOWN},
	{"MLEOK",              MLEOK},
	{"MLEDEAD",            MLEDEAD},
	{"MLEGBAD",            MLEGBAD},
	{"MLEGSEQ",            MLEGSEQ},
	{"MLEPBTK",            MLEPBTK},
	{"MLEPBIG",            MLEPBIG},
	{"MLEOVFL",            MLEOVFL},
	{"MLEMEM",             MLEMEM},
	{"MLEACCEPT",          MLEACCEPT},
	{"MLECONNECT",         MLECONNECT},
	{"MLECLOSED",          MLECLOSED},
	{"MLEDEPTH",           MLEDEPTH},
	{"MLENODUPFCN",        MLENODUPFCN},
	{"MLENOACK",           MLENOACK},
	{"MLENODATA",          MLENODATA},
	{"MLENOTDELIVERED",    MLENOTDELIVERED},
	{"MLENOMSG",           MLENOMSG},
	{"MLEFAILED",          MLEFAILED},
	{"MLEGETENDEXPR",      MLEGETENDEXPR},
	{"MLEPUTENDPACKET",    MLEPUTENDPACKET},
	{"MLENEXTPACKET",      MLENEXTPACKET},
	{"MLEUNKNOWNPACKET",   MLEUNKNOWNPACKET},
	{"MLEGETENDPACKET",    MLEGETENDPACKET},
	{"MLEABORT",           MLEABORT},
	{"MLEMORE",            MLEMORE},
	{"MLENEWLIB",          MLENEWLIB},
	{"MLEOLDLIB",          MLEOLDLIB},
	{"MLEBADPARAM",        MLEBADPARAM},
	{"MLENOTIMPLEMENTED",  MLENOTIMPLEMENTED},
	{"MLEINIT",            MLEINIT},
	{"MLEARGV",            MLEARGV},
	{"MLEPROTOCOL",        MLEPROTOCOL},
	{"MLEMODE",            MLEMODE},
	{"MLELAUNCH",          MLELAUNCH},
	{"MLELAUNCHAGAIN",     MLELAUNCHAGAIN},
	{"MLELAUNCHSPACE",     MLELAUNCHSPACE},
	{"MLENOPARENT",        MLENOPARENT},
	{"MLENAMETAKEN",       MLENAMETAKEN},
	{"MLENOLISTEN",        MLENOLISTEN},
	{"MLEBADNAME",         MLEBADNAME},
	{"MLEBADHOST",         MLEBADHOST},
	{"MLERESOURCE",        MLERESOURCE},
	{"MLELAUNCHFAILED",    MLELAUNCHFAILED},
	{"MLELAUNCHNAME",      MLELAUNCHNAME},
	{"MLEPDATABAD",        MLEPDATABAD},
	{"MLEPSCONVERT",       MLEPSCONVERT},
	{"MLEGSCONVERT",       MLEGSCONVERT},
	{"MLETRACEON",         MLETRACEON},
	{"MLETRACEOFF",        MLETRACEOFF},
	{"MLEDEBUG",           MLEDEBUG},
	{"MLEASSERT",          MLEASSERT},
	{"MLEUSER",            MLEUSER},
	{(char *)0}
};

static message_code mathlink_ErrorMessages[] = {
	{"Unknown mathlink error message",                                                       MLEUNKNOWN},
	{"Everything OK",                                                                        MLEOK},
	{"Link Dead",                                                                            MLEDEAD},
	{"Link read inconsistant data",                                                          MLEGBAD},
	{"Get out of sequence",                                                                  MLEGSEQ},
	{"PutNext passed bad token",                                                             MLEPBTK},
	{"Put out of sequence",                                                                  MLEPSEQ},
	{"PutData given too much data",                                                          MLEPBIG},
	{"Machine number overflow",                                                              MLEOVFL},
	{"Out of memory",                                                                        MLEMEM},
	{"Failure to accept socket connection",                                                  MLEACCEPT},
	{"Deferred connection still unconnected",                                                MLECONNECT},
	{"The other side of the connection closed the link, you may yet get undelivered data",   MLECLOSED},
	{"Internal mathlink library error",                                                      MLEDEPTH},
	{"Link cannot be duplicated",                                                            MLENODUPFCN},
	{"No acknowlodgement?",                                                                  MLENOACK},         /* Investigate this */
	{"No Data?",                                                                             MLENODATA},        /* Investigate this */
	{"Packet Not delivered?",                                                                MLENOTDELIVERED},  /* Investigate this */
	{"No Message?",                                                                          MLENOMSG},         /* Investigate this */
	{"Failed?",                                                                              MLEFAILED},        /* Investigate this */
	{"Unknown",                                                                              MLEGETENDEXPR},    /* Investigate this */
	{"Unexpected call of PutEndPacket",                                                      MLEPUTENDPACKET},
	{"NextPacket called while current packet has unread data",                               MLENEXTPACKET},
	{"NextPacket read in an unknown packet head",                                            MLEUNKNOWNPACKET},
	{"Unexpected end of packet",                                                             MLEGETENDPACKET},
	{"A put or get was aborted before affecting the link",                                   MLEABORT},
	{"Internal mathlink library error",                                                      MLEMORE},
	{"Unknown",                                                                              MLENEWLIB},        /* Investigate this */
	{"Unknown",                                                                              MLEOLDLIB},        /* Investigate this */
	{"Unknown",                                                                              MLEBADPARAM},      /* Investigate this */
	{"Feature not currently implemented",                                                    MLENOTIMPLEMENTED},
	{"Mathlink environment not initialized",                                                 MLEINIT},
	{"Insufficient arguments to open link",                                                  MLEARGV},
	{"Protocol unavailable",                                                                 MLEPROTOCOL},
	{"Mode unavailable",                                                                     MLEMODE},
	{"Launch unsupported",                                                                   MLELAUNCH},
	{"Cannot launch the program again from the same file",                                   MLELAUNCHAGAIN},
	{"Insufficient space to launch the program",                                             MLELAUNCHSPACE},
	{"Found no parent to connect to",                                                        MLENOPARENT},
	{"Link name already in use",                                                             MLENAMETAKEN},
	{"Link name not found to be listening",                                                  MLENOLISTEN},
	{"Link name missing or not in proper form",                                              MLEBADNAME},
	{"Location unreachable or not in proper form",                                           MLEBADHOST},
	{"A required resource is unavaible",                                                     MLERESOURCE},
	{"Program failed to launch due to a missing resource or library",                        MLELAUNCHFAILED},
	{"Launch failed because of inability to find program",                                   MLELAUNCHNAME},
	{"unable to convert from given character encoding to link encoding",                     MLEPSCONVERT},
	{"unable to convert from link encoding to requested encoding",                           MLEGSCONVERT},
	{"character data in given encoding incorrect",                                           MLEPDATABAD},
	{"Unknown mathlink internal",                                                            MLETRACEON},
	{"Unknown mathlink internal",                                                            MLETRACEOFF},
	{"Unknown mathlink internal",                                                            MLEDEBUG},
	{"Failure of an internal assertion",                                                     MLEASSERT},
	{"Start of user defined errors",                                                         MLEUSER},
	{(char *)0}
};

static PyObject *ErrorDictionary;

static message_code mathlink_PacketDescription[] = {
	{"IllegalPacket",          ILLEGALPKT},
	{"CallPacket",             CALLPKT},
	{"EvaluatePacket",         EVALUATEPKT},
	{"ReturnPacket",           RETURNPKT},
	{"InputNamePacket",        INPUTNAMEPKT},
	{"Enter",                  ENTERTEXTPKT},
	{"EnterTextPacket",        ENTERTEXTPKT},
	{"EnterExpressionPacket",  ENTEREXPRPKT},
	{"OutputNamePacket",       OUTPUTNAMEPKT},
	{"ReturnTextPacket",       RETURNTEXTPKT},
	{"ReturnExpressionPacket", RETURNEXPRPKT},
	{"DisplayPacket",          DISPLAYPKT},
	{"DisplayEndPacket",       DISPLAYENDPKT},
	{"MessagePacket",          MESSAGEPKT},
	{"TextPacket",             TEXTPKT},
	{"InputPacket",            INPUTPKT},
	{"InputStringPacket",      INPUTSTRPKT},
	{"MenuPacket",             MENUPKT},
	{"SyntaxPacket",           SYNTAXPKT},
	{"ErrorPacket",            SYNTAXPKT},
	{"SuspendPacket",          SUSPENDPKT},
	{"ResumePacket",           RESUMEPKT},
	{"BeginDialogPacket",      BEGINDLGPKT},
	{"EndDialogPacket",        ENDDLGPKT},
	{(char *)0}
};

static message_code mathlink_PacketTitle[] = {
	{"ILLEGALPKT",      ILLEGALPKT},
	{"CALLPKT",         CALLPKT},
	{"EVALUATEPKT",     EVALUATEPKT},
	{"RETURNPKT",       RETURNPKT},
	{"INPUTNAMEPKT",    INPUTNAMEPKT},
	{"ENTERTEXTPKT",    ENTERTEXTPKT},
	{"ENTEREXPRPKT",    ENTEREXPRPKT},
	{"OUTPUTNAMEPKT",   OUTPUTNAMEPKT},
	{"RETURNTEXTPKT",   RETURNTEXTPKT},
	{"RETURNEXPRPKT",   RETURNEXPRPKT},
	{"DISPLAYPKT",      DISPLAYPKT},
	{"DISPLAYENDPKT",   DISPLAYENDPKT},
	{"MESSAGEPKT",      MESSAGEPKT},
	{"TEXTPKT",         TEXTPKT},
	{"INPUTPKT",        INPUTPKT},
	{"INPUTSTRPKT",     INPUTSTRPKT},
	{"MENUPKT",         MENUPKT},
	{"SYNTAXPKT",       SYNTAXPKT},
	{"SUSPENDPKT",      SUSPENDPKT},
	{"RESUMEPKT",       RESUMEPKT},
	{"BEGINDLGPKT",     BEGINDLGPKT},
	{"ENDDLGPKT",       ENDDLGPKT},
	{(char *)0}
};

message_code linkmodes[] = {
	{ "loopback",      LOOPBACKBIT},
	{ "launch",        LAUNCHBIT},
	{ "parentconnect", PARENTCONNECTBIT},
	{ "listen",        LISTENBIT},
	{ "connect",       CONNECTBIT},
	{ "read",          READBIT},
	{ "write",         WRITEBIT},
	{ "server",        SERVERBIT},
	{ (char*)0,(int)0}
};

static message_code mathlink_TokenCodes[] = {
	{"MLTK_MLSHORT",            MLTK_MLSHORT},
	{"MLTK_MLINT",              MLTK_MLINT},
	{"MLTK_MLLONG",             MLTK_MLLONG},
	{"MLTK_MLFLOAT",            MLTK_MLFLOAT},
	{"MLTK_MLDOUBLE",           MLTK_MLDOUBLE},
	{"MLTK_MLLONGDOUBLE",       MLTK_MLLONGDOUBLE},
	{"MLTKSTR",                 MLTKSTR},
	{"MLTKOLDSTR",              MLTKOLDSTR},
	{"MLTKSYM",                 MLTKSYM},
	{"MLTKOLDSYM",              MLTKOLDSYM},
	{"MLTKERROR",               MLTKERROR},
	{"MLTKFUNC",                MLTKFUNC},
	{"MLTKREAL",                MLTKREAL},
	{"MLTKOLDREAL",             MLTKOLDREAL},
	{"MLTKINT",                 MLTKINT},
	{"MLTKOLDINT",              MLTKOLDINT},
	{(char *) 0}
};

static message_code mathlink_MessageCodes[] = {
	{"MLTerminateMessage",           MLTerminateMessage},
	{"MLInterruptMessage",           MLInterruptMessage},
	{"MLAbortMessage",               MLAbortMessage},
	{"MLEndPacketMessage",           MLEndPacketMessage},
	{"MLSynchronizeMessage",         MLSynchronizeMessage},
	{"MLImDyingMessage",             MLImDyingMessage},
	{"MLWaitingAcknowledgment",      MLWaitingAcknowledgment},
	{"MLMarkTopLevelMessage",        MLMarkTopLevelMessage},
	{"MLFirstUserMessage",           MLFirstUserMessage},
	{"MLFirstUserMessage",           MLFirstUserMessage},
	{(char *) 0},
};

static message_code mathlink_DialogFunctions[] = {
	{"MLAlertFunction",               MLAlertFunction},
	{"MLRequestFunction",             MLRequestFunction},
	{"MLConfirmFunction",             MLConfirmFunction},
	{"MLRequestArgvFunction",         MLRequestArgvFunction},
	{"MLRequestToInteractFunction",   MLRequestToInteractFunction},
	{(char *)0},
};

static message_code mathlink_DeviceTypes[] = {
	{"UNREGISTERED_TYPE",      UNREGISTERED_TYPE},
	{"UNIXPIPE_TYPE",          UNIXPIPE_TYPE},
	{"UNIXSOCKET_TYPE",        UNIXSOCKET_TYPE},
#if UNIX_MATHLINK
	{"UNIXSHM_TYPE",           UNIXSHM_TYPE},
#endif
	{"LOOPBACK_TYPE",          LOOPBACK_TYPE},
#ifdef WINDOWS_MATHLINK
	{"WINLOCAL_TYPE",          WINLOCAL_TYPE},
	{"WINFMAP_TYPE",           WINFMAP_TYPE},
	{"WINSHM_TYPE",            WINSHM_TYPE},
#endif
	{(char *)0},
};

static message_code mathlink_DeviceSelectors[] = {
	{"MLDEVICE_TYPE",             MLDEVICE_TYPE},
	{"MLDEVICE_NAME",             MLDEVICE_NAME},
	{"MLDEVICE_WORLD_ID",         MLDEVICE_WORLD_ID},
	{"PIPE_FD",                   PIPE_FD},
	{"PIPE_CHILD_PID",            PIPE_CHILD_PID},
	{"SOCKET_FD",                 SOCKET_FD},
	{"SOCKET_PARTNER_ADDR",       SOCKET_PARTNER_ADDR},
	{"SOCKET_PARTNER_PORT",       SOCKET_PARTNER_PORT},
	{(char *)0}
};
	

static PyObject *PacketDescriptionDictionary;
static PyObject *PacketDictionary;
static PyObject *MessageCodesDictionary;

/****************** Function Prototypes ********************/

static int mathlink_EnvAllowThreadsToRun(mathlink_Env *self);
static int mathlink_LinkAllowThreadsToRun(mathlink_Link *self);
void mathlink_AddMessageCodesToDict(PyObject *Dict, message_code *message,int swtch);
void mathlink_SetErrorConditionFromLink(mathlink_Link *Link);
void mathlink_SetErrorConditionFromCode(int code);
int checkforerror(long result, long expected, mathlink_Link *self,int selector);
static PyObject * mathlink_ClearError(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_Error(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_ErrorString(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_DefaultErrBehavior(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutInteger(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetInteger(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutLong(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetLong(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutFloat(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetFloat(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutString(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetString(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutByteString(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetByteString(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutUCS2String(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetUCS2String(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutUTF8String(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetUTF8String(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutUTF16String(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetUTF16String(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutUTF32String(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetUTF32String(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_Put7BitCharacters(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutComplex(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetComplex(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutNumber(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetNumber(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutSymbol(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetSymbol(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutUCS2Symbol(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetUCS2Symbol(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutUTF8Symbol(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetUTF8Symbol(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutUTF16Symbol(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetUTF16Symbol(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutUTF32Symbol(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetUTF32Symbol(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutIntegerList(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetIntegerList(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutFloatList(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetFloatList(mathlink_Link* self, PyObject *args);
static PyObject * build_row(void* array, int* offset, long* dimensions, int dim, int depth, int isReal);
static PyObject * mathlink_GetIntArray(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetFloatArray(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutSize(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutData(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetData(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutRawSize(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutRawData(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetRawData(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_BytesToGet(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_RawBytesToGet(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_BytesToPut(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_NewPacket(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_EndPacket(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_NextPacket(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutFunction(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetFunction(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_CheckFunction(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutType(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetType(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetRawType(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_PutNext(mathlink_Link *self, PyObject *args);
static PyObject *mathlink_GetNext(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_GetNextRaw(mathlink_Link *self, PyObject *args);
static PyObject *mathlink_PutArgCount(mathlink_Link *self, PyObject *args);
static PyObject *mathlink_GetArgCount(mathlink_Link *self, PyObject *args);
static PyObject *mathlink_GetRawArgCount(mathlink_Link *self, PyObject *args);
MLDEFN(int, mathlink_YieldFunctionHandler,(MLINK lp, MLYieldParameters yp));
static PyObject *mathlink_SetYieldFunction(mathlink_Link *self, PyObject *args);
static PyObject *mathlink_YieldFunction(mathlink_Link *self, PyObject *args);
static int mathlink_DoMessage(void *so);
MLDEFN(void, mathlink_MessageFunctionHandler,(MLINK lp, int msg, int mark));
static PyObject *mathlink_SetMessageHandler(mathlink_Link *self, PyObject *args);
static PyObject *mathlink_MessageHandler(mathlink_Link *self, PyObject *args);
static PyObject *mathlink_GetMessage(mathlink_Link *self, PyObject *args);
static PyObject *mathlink_MessageReady(mathlink_Link *self, PyObject *args);
static PyObject *mathlink_PutMessage(mathlink_Link *self,PyObject *args);
static PyObject * mathlink_CreateMark(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_SeekToMark(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_SeekMark(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_TransferExpression(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_Ready(mathlink_Link *self, PyObject *args);
MLINK mathlink_NewLink(mathlink_Env *self, PyObject *args, PyObject *keywords);
static PyObject * mathlink_GetName(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_SetName(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_Flush(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_Duplicate(mathlink_Link *self, PyObject *args);
static mathlink_Link * mathlink_Open(mathlink_Env *self, PyObject *args, PyObject *keywords);
static mathlink_Link * mathlink_OpenString(mathlink_Env *self, PyObject *args);
static mathlink_Link * mathlink_OpenArgv(mathlink_Env *self, PyObject *args);
static mathlink_Link * mathlink_LoopbackOpen(mathlink_Env *self, PyObject *args);
static PyObject * mathlink_Connect(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_EstablishString(mathlink_Link *self, PyObject *args, PyObject *keywords);
static PyObject * mathlink_DeviceInformation(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_FeatureString(mathlink_Link *self, PyObject *args);
static PyObject * mathlink_Close(mathlink_Link *self, PyObject *args);
static mathlink_Link * Link_Alloc(mathlink_Env *self);
static void Link_Dealloc(mathlink_Link *self);
static PyObject * Link_Getattr(mathlink_Link *self, char *name);
static PyObject * Link_Repr(mathlink_Link *self);
static PyObject * mathlink_SetDefaultYieldFunction(mathlink_Env *self, PyObject *args);
static PyObject * mathlink_DefaultYieldFunction(mathlink_Env *self, PyObject *args);
static PyObject * mathlink_GetLink(mathlink_Mark *self, PyObject *args);
int mathlink_AddIDToList(MLINK lp);
static mathlink_Link * mathlink_Align(PyObject *self, PyObject *args, PyObject *keywords);
static mathlink_Env * Env_Alloc(PyObject *self, PyObject *args);
static PyObject * Env_Getattr(mathlink_Env *self, char *name);
static PyObject * Env_Repr(mathlink_Env *self);
static void Env_Dealloc(mathlink_Env *self);
static mathlink_Link * mathlink_ReadyParallel(mathlink_Env *self, PyObject *args);
mldlg_result mathlink_AlertStub(MLENV ep, kcharp_ct message);
mldlg_result mathlink_ConfirmStub(MLENV ep, kcharp_ct question, mldlg_result defaultanswer);
mldlg_result mathlink_RequestStub(MLENV ep, kcharp_ct prompt, charp_ct response, long size);
mldlg_result mathlink_RequestArgvStub(MLENV ep, charpp_ct argv, long len, charp_ct buff, long size);
mldlg_result mathlink_RequestToInteractStub(MLENV ep, mldlg_result wait);
static PyObject * mathlink_Alert(mathlink_Env *self, PyObject *args);
static PyObject * mathlink_Confirm(mathlink_Env *self, PyObject *args);
static PyObject * mathlink_Request(mathlink_Env *self, PyObject *args);
static PyObject * mathlink_RequestArgv(mathlink_Env *self, PyObject *args);
static PyObject * mathlink_RequestToInteract(mathlink_Env *self, PyObject *args);
static PyObject * mathlink_SetDialogFunction(mathlink_Env *self, PyObject *args);
static PyObject * mathlink_RequestBufferSize(PyObject *self, PyObject *args);
static PyObject * mathlink_SetRequestBufferSize(PyObject *self, PyObject *args);
static PyObject * mathlink_ArgvSize(PyObject *self, PyObject *args);
static PyObject * mathlink_SetArgvSize(PyObject *self, PyObject *args);
static void Mark_Dealloc(mathlink_Mark *self);
static PyObject * Mark_Getattr(mathlink_Mark *self, char *name);
static PyObject * Mark_Repr(mathlink_Mark *self);
MLDEFN(void *, mathlink_MemMallocWrapper, (size_t bytes));
MLDEFN(void, mathlink_MemFreeWrapper, (void *bytes));
void * mathlink_InitLinkID(void);
void initmathlink(void);

/****************** Doc strings ********************/
static char mathlink_ClearError__doc[] =
"The clearerror method clears the current error status of a link object and resets the internal value to MLEOK.";

static char mathlink_Error__doc[] = 
"The error method returns the current error status of a link object.";

static char mathlink_ErrorString__doc[] =
"The errorstring method returns a string message detailing the type of error.";

static char mathlink_DefaultErrBehavior__doc[] =
"The errorbehavior method sets an internal flag in the link object that\n\
specifies whether or not to automatically clear link errors.  The method\n\
takes one argument, an Integer value.  If the argument is greater than zero\n\
the default error behavior for the link is to clear all errors after raising\n\
an exception.  If the argument is less than or equal to zero the default\n\
behavior of the link is to leave the error condition set.  This state requires\n\
that the programmer explicitly clear the error using the clearerror method.";

static char mathlink_PutInteger__doc[] =
"The putinteger method transfers an integer across a link.  The method takes one\n\
argument, an integer.";

static char mathlink_GetInteger__doc[] =
"The getint method returns an integer object read from the link.  The method takes\n\
no arguments.";

static char mathlink_PutLong__doc[] =
"The putlong method transfers a long integer object across a link.  The method\n\
takes one argument, a Long integer";

static char mathlink_GetLong__doc[] = 
"The getlong method returns a Long integer read from the link.  The method\n\
takes no arguments.";

static char mathlink_PutFloat__doc[] =
"The putfloat method transfers a Float object(FloatType number, floating point\n\
number) across a link.  The method takes one argument, a Float object.";

static char mathlink_GetFloat__doc[] =
"The getfloat method returns a Float object(FloatType, floating point number\n\
)read from a link. The method takes no arguments.";

static char mathlink_PutString__doc[] =
"The putstring method transfers an ascii encoded string across a link.  The method\n\
takes one argument an ascii encoded String object.  The behavior of the method is\n\
undefined for non-ascii encodings at this time.  The ASCII string must be composed of\n\
non escaped characters.";

static char mathlink_GetString__doc[] =
"The getstring method returns an ascii encoded string object retrieved from a link.\n\
The method takes no arguments.";

static char mathlink_PutByteString__doc[] = 
"The putbytestring method puts a string of characters across a link.  These strings differ\n\
from a generic ASCII only string in that special characters such as escaped tabs, newlines\n\
etc... are supported.  For example you could use putbytestring to transfer the following string:\n\
Name\\tBirthdate\\tSocial Security Number\\n----\\t---------\\t----------------------\\n\n\
The method takes one argument, an ASCII encoded string object.";

static char mathlink_GetByteString__doc[] = 
"The getbytestring method retrieves a string from a link object.  The string allows for escaped\n\
characters such as \\t,\\n,etc...";

static char mathlink_PutUCS2String__doc[] = 
"The putucs2string method puts a UCS-2 encoded string across a link.  The\n\
method takes one argument, a unicode encoded string object.  The Python\n\
Unicode string type implements UCS-2 (A subset of full UTF-16 that implements\n\
characters in the Unicode BMP).";

static char mathlink_GetUCS2String__doc[] = 
"The getucs2string method returns a UCS-2 encoded string from a link object.\n\
The method takes no arguments.  The Python Unicode string type implements UCS-2\n\
(A subset of full UTF-16 that implements characters in the Unicode BMP).";

static char mathlink_PutUTF8String__doc[] =
"The pututf8string method puts a UTF-8 encoded string across a link.  Typically\n\
the programmer would use this method to transfer raw string data as read from\n\
a file across the link.";

static char mathlink_GetUTF8String__doc[] =
"The getutf8string method receives a raw byte string from the link that is encoded\n\
as UTF-8 character data.";

static char mathlink_PutUTF16String__doc[] =
"The pututf16string method puts a UTF-16 encoded string across a link.  This method\n\
can transfer the Python Unicode string type.";

static char mathlink_GetUTF16String__doc[] =
"The getutf16string method receives a string from the link that is encoded as UTF-16\n\
characters.  This function implements support for full UTF-16 characters including Unicode\n\
surrogate pairs.  This function may not function correctly when trying to store the returned\n\
results as a Python Unicode string.";

static char mathlink_PutUTF32String__doc[] =
"The pututf32string puts a UTF-32 encoded string across a link.  Typically the\n\
programmer would use this method to transfer raw string data as read from\n\
a file across the link.";

static char mathlink_GetUTF32String__doc[] =
"The getutf32string receives a raw byte string from the link that encodes characters in\n\
the UTF-32 character encoding form.";

static char mathlink_Put7BitCharacters__doc[] = 
"The put7bitcharacters method puts a string object to think.  The method takes two arguments:\n\
1. The string to put, and 2. The number of characters yet to put.";

static char mathlink_PutComplex__doc[] = 
"The putcomplex method transfers a complex number object across a link.  The\n\
method takes one argument; a complex number object.";

static char mathlink_GetComplex__doc[] = 
"The getcomplex method returns a complex number from a link object.  The\n\
method takes no arguments.";

static char mathlink_PutNumber__doc[] = 
"The putnumber method puts any valid basic python numerical type across a link object.  The\n\
method takes one argument that can from the following: integer object, long object, float\n\
object, complex object.";

static char mathlink_GetNumber__doc[] = 
"The getnumber method returns a python numeric object from a link object.  The\n\
number returned can be an object of a type from the following list: long object\n\
(integer objects are converted to longs automatically), float object, or a complex\n\
object.";

static char mathlink_PutSymbol__doc[] = 
"The putsymbol method transfers the name of a symbol across a link object.  The\n\
method takes one argument, the symbol name, as a string object.";

static char mathlink_GetSymbol__doc[] = 
"The getsymbol method returns a symbol name from a link object.  The method\n\
takes no arguments and returns a string object.";

static char mathlink_PutUCS2Symbol__doc[] = 
"The putucs2symbol method puts the name of a Mathematica symbol across a link object.  The\n\
method takes one argument, the name of the symbol, encoded as a UCS-2 string object.  The\n\
Python Unicode string type encodes characters as UCS-2.";

static char mathlink_GetUCS2Symbol__doc[] = 
"The getucs2symbol method returns the name of a Mathematica symbol from a link object.  The\n\
method takes no arguments and returns the name of the symbol as a UCS-2 encoded string object.";

static char mathlink_PutUTF8Symbol__doc[] =
"The pututf8symbol method puts the name of a Mathematica symbol across a link.  The symbol\n\
should be encoded as a byte string in the UTF-8 encoding form.";

static char mathlink_GetUTF8Symbol__doc[] =
"The getuf8symbol method returns the name of a Mathematica symbol from a link object.  The\n\
symbol name is a byte string encoded in the UTF-8 encoding form.";

static char mathlink_PutUTF16Symbol__doc[] =
"The pututf16symbol method puts the name of a Mathematica symbol across a link.  The symbol\n\
should be encoded as a byte string in the UTF-16 encoding form.";

static char mathlink_GetUTF16Symbol__doc[] =
"The getuf16symbol method returns the name of a Mathematica symbol from a link object.  The\n\
symbol name is a byte string encoded in the UTF-16 encoding form.";

static char mathlink_PutUTF32Symbol__doc[] =
"The pututf32symbol method puts the name of a Mathematica symbol across a link.  The symbol\n\
should be encoded as a byte string in the UTF-32 encoding form.";

static char mathlink_GetUTF32Symbol__doc[] =
"The getutf32symbol method returns the name of a Mathematica symbol from a link object.  The\n\
symbol name is a byte string encoded in the UTF-32 encoding form.";

static char mathlink_PutIntegerList__doc[] = 
"The putintlist method puts a list of integer objects across a link object.  The\n\
method takes one argument, a one level list of integers.  Here is an example of\n\
a valid list: [1,2,3,4,5,6].";

static char mathlink_GetIntegerList__doc[] = 
"The getintlist method returns a list of integer objects from a link object.\n\
The method takes no arguments.";

static char mathlink_PutFloatList__doc[] = 
"The putfloatlist method puts a list of float objects across a link object.  The\n\
method takes one argument, a one level list of floats.  Here is an example of\n\
a valid list: [1.1,2.0,3.5,5.1,6.2].";

static char mathlink_GetFloatList__doc[] = 
"The getfloatlist method returns a list of float objects from a link objects.\n\
The method takes not arguments.";

static char mathlink_GetIntArray__doc[] = 
"The getintarray method returns a list of nested integer lists.  The method\n\
takes no arguments and returns a sequence object.";

static char mathlink_GetFloatArray__doc[] = 
"The getintarray method returns a list of nested float lists.  The method\n\
takes no arguments and returns a sequence object.";

static char mathlink_PutSize__doc[] =
"The putsize method ...(finish)";

static char mathlink_PutData__doc[] =
"The putdata method ...(finish)";

static char mathlink_GetData__doc[] =
"The getdata method ...(finish)";

static char mathlink_PutRawSize__doc[] =
"The putrawsize method ...(finish)";

static char mathlink_PutRawData__doc[] =
"The putrawdata method...(finish)";

static char mathlink_GetRawData__doc[] =
"The getrawdata method ...(finish)";

static char mathlink_BytesToGet__doc[] =
"The bytestoget method returns the number of bytes left to get in the current\n\
packet.";

static char mathlink_RawBytesToGet__doc[] =
"The rawbytestoget method returns the number of bytes left to get in the current\n\
packet.";

static char mathlink_BytesToPut__doc[] =
"The bytestoput method returns the number of bytes remaining to send on the\n\
composite packet.";

static char mathlink_NewPacket__doc[] = 
"The newpacket method discards the contents of the current packet or expression\n\
from the link and makes the link object ready for the next packet.";

static char mathlink_EndPacket__doc[] = 
"The endpacket method sends an end-of-packet indicator across a link object.\n\
The method takes no arguments.";

static char mathlink_NextPacket__doc[] = 
"The nextpacket returns the packet type of next packet coming across a link object.\n\
The method takes no arguments and returns an integer object representing the packet type.";

static char mathlink_PutFunction__doc[] = 
"The putfunction method puts a the name of a function and the number of arguments across\n\
a link object.  The method takes two arguments. The first argument should be a string\n\
object representing the name of the function.  The second argument should be an integer\n\
object representing the number of arguments that will be sent across the link object for\n\
the function.";

static char mathlink_GetFunction__doc[] = 
"The getfunction method returns the name of a function and the number of arguments for that\n\
that function from a link object.  The method returns the name and the argument number as a tuple\n\
object with two members: a string object representing the function name, and an integer object\n\
representing the number of arguments.  The method takes no arguments.";

static char mathlink_CheckFunction__doc[] =
"The checkfunction method takes as an argument the name of the expected incoming function\n\
and checks that the actual incoming function name matches the expected function name.";

static char mathlink_PutType__doc[] = 
"The puttype method puts a token type across a link object.  The method\n\
takes one argument, a long object, representing the token type.  You may\n\
use the integer number or the constant such as MLTKINT, MLTKREAL, etc...";

static char mathlink_GetType__doc[] = 
"The gettype method gets a token type from a link object.  The method takes no\n\
arguments and returns a long object representing the type of the next token available\n\
from the link object.  The method returns the token type as an integer which you may\n\
match with constants such as MLTKINT, MLTKREAL, etc...";

static char mathlink_GetRawType__doc[] = 
"The getrawtype method returns a token type from a link object.  The token returned\n\
by this method represents mathlink's internal notion of the token.  These tokens are\n\
represented by constants such as MLTK_MLINT, MLTK_MLFLOAT, MLTK_MLSHORT, etc...";

static char mathlink_PutNext__doc[] = 
"The putnext method specifies the type to transfer across the link object.\n\
The types allowed are tokens as returned by MLGetType.  You may use a long object\n\
or the corresponding constant declaration.";

static char mathlink_GetNext__doc[] = 
"The getnext method returns an integer object corresponding to the token type of\n\
the next value coming off the link.  The integer objects match the constants MLTKINT,\n\
MLTKREAL, etc...  The method does not take any arguments.";

static char mathlink_GetNextRaw__doc[] =
"The getnextraw method returns an integer object corresponding to the token type of\n\
the next value coming off the link.  The integer objects match the constants MLTKINT,\n\
MLTKREAL, etc...  The method does not take any arguments.";

static char mathlink_PutArgCount__doc[] = 
"The putargcount method specifies the number of arguments of a composite function\n\
to be put across a link object.  The method takes one argument, an integer object\n\
specifying the number of arguments.";

static char mathlink_GetArgCount__doc[] = 
"The getargcount method returns an integer object representing the number of arguments\n\
to a function currently in transit across a link object.  The method takes no arguments.";

static char mathlink_GetRawArgCount__doc[] =
"The getrawargcount method returns an integer object representing the number of arguments\n\
to a function currently in transit across a link object.  The method takes no arguments.";

static char mathlink_SetYieldFunction__doc[] = 
"The setyieldfunction method allows the programmer to set a yield function for a given\n\
link object.  The method takes one argument, a callable object, function, or object method.";

static char mathlink_YieldFunction__doc[] = 
"The yieldfunction method returns the yield function currently set for a link object.\n\
If the link object does not have a yield function set, then the method returns None.\n\
The method takes no arguments.";

static char mathlink_SetMessageHandler__doc[] = 
"The setmessagehandler method sets the message handler for a given link object.  The\n\
method takes one argument, a callable object, function, or object method.";

static char mathlink_MessageHandler__doc[] = 
"The messagehandler method returns the current message handler function, object method, or callable\n\
object set as a message handler for the given link object.  If the link object does not have a message\n\
handler installed, the method returns None.  The method takes no arguments.";

static char mathlink_GetMessage__doc[] = 
"The getmessage method asks a link objects device to return the last available message.  The\n\
method takes no arguments and returns a tuple.";

static char mathlink_MessageReady__doc[] = 
"The message ready method interrogates a link objects device for the availability of\n\
a message.  The method takes no arguments.";

static char mathlink_PutMessage__doc[] = 
"The put message method instructs a link objects device to transmit an out-of-band message.  The\n\
method takes one argument, a long object representing the integer value of the message.  MathLink\n\
defines several possible values for messages such as MLTerminateMessage, MLInterruptMessage, etc...\n\
However, outside of those constants, messages are largely left to definition and interpretation by\n\
the programmer.";

static char mathlink_CreateMark__doc[] = 
"The createmark method creates a new mark object pointing to the current value/index of the data\n\
in a link object's data stream.  A mark object allows you to continue reading/writing data to a link\n\
object and then return to the earlier location in the data stream at a later time.  The method returns\n\
a mark object.";

static char mathlink_SeekToMark__doc[] = 
"The seektomark method allows a programmer to return to some offset in the link object's data\n\
stream specified by an index value.  The method takes two arguments; a mark object, and an integer\n\
object representing the value of offset forward into the stream from the mark object.";

static char mathlink_SeekMark__doc[] = "Please see the documentation for the seektomark method.";

static char mathlink_TransferExpression__doc[] = 
"The transferexpression method transfers an expression from one link object to another.  The\n\
method takes one argument, the destination link object.  The link objects need not be distinct.\n\
The link objects may be loopback or ordinary links.";

static char mathlink_Ready__doc[] = 
"The ready method tests whether or not there is data on the link object to read\n\
The ready method does not block.  You must call the flush method before calling\n\
ready().  The method takes no arguments and returns an integer object.";

static char mathlink_GetName__doc[] = 
"The getname method returns the name of a link object.  The name usually depends\n\
on the device used by the link object.  For a socket device the name method would\n\
return port@hostname.  If the link object does not have a name, then the method returns\n\
None.";

static char mathlink_SetName__doc[] = 
"The setname method allows the programmer to explicitly set the name of a link object\n\
after the link is open and connected.";

static char mathlink_Flush__doc[] = 
"The flush method flushes out any buffers containing data waiting for transmission\n\
across a link object.";

static char mathlink_Duplicate__doc[] = 
"The duplicate method creates returns a copy of a link object.  The method takes one argument,\n\
a string object representing the name of the new link object.";

static char mathlink_Open__doc[] = 
"The open method opens the internal MathLink interface to a new connection.\n\
The method accepts the following keywords:\n\n\
name           - The name of link.(eg, 10233@remotehost.domain.com, math -mathlink)\n\
protocol       - The name of protocol to use.(eg, SharedMemory, TCPIP)\n\
mode           - The mode of connection. (eg, Launch, Connect)\n\
host           - The host name of the remote connection.\n\
options        - The set of desired options for the new link. (eg MLForceYield)\n\
create         - Corresponds to -linkcreate from a command line invocation. (eg create=1)\n\
connect        - Corresponds to -linkconnect from a command line invocation. (eg connect=1)\n\
launch         - Corresponds to -linklaunch from a command line invocation. (eg launch=1)\n\
authentication - TO BE DESCRIBED.\n\
device         - TO BE DESCRIBED.";

static char mathlink_OpenString__doc[] =
"The openstring method creates a linkobject using a parameters string that contains all the\n\
link arguments such as they might be found on the command line.  For example:\n\
\n\
-linkname 'math -mathlink' -linkmode launch -linkprotocol SharedMemory\n\
";

static char mathlink_OpenArgv__doc[] = 
"The openargv method opens a linkobject using a tuple of arguments such as those found on a\n\
command line.  The function takes one argument a list or tuple.  A common use of this\n\
method would be the following:\n\
\n\
\n\
import sys, mathlink\n\
\n\
a = mathlink.link()\n\
a.openargv(sys.argv)";

static char mathlink_LoopbackOpen__doc[] = 
"The loopbackopen method opens a link object as a loopback link.";

static char mathlink_Connect__doc[] = 
"The connect method connects the local link object with its remote connection.  It\n\
initializes internal communication standards and synchronizes the communication streams.";

static char mathlink_EstablishString__doc[] = 
"The establishstring method allows a programmer to set up a local link object's details about\n\
the remote side of a connection without relying on MathLink to exchange the information.  The method\n\
takes the following optional arguments: mathlink, decoders, numericsid, tokens, textid, and formats.\n\
Mathlink and formats should be integer objects. Decoders, numericsid, tokens, and textid should be\n\
string objects.";

static char mathlink_DeviceInformation__doc[] = 
"The deviceinformation method allows a programmer to interrogate a link object's device\n\
for configuration information.  The method takes one argument, an integer object that\n\
acts as a selector.  The function returns a value based upon the selector(ie based upon the\n\
type of information requested from the device).\n\
\n\
MLDEVICE_TYPE          -  Returns an integer object.(Represents a constant value specific to the\n\
	                      type of the device. Eg UNIXPIPE_TYPE.)\n\
MLDEVICE_NAME          -  Returns a string object. The string holds the current name of the connection.\n\
MLDEVICE_WORLD_ID      -  Returns a string object\n\
PIPE_FD              -  Returns an integer object. (The number corresponds to the file descriptor\n\
	                      number of the pipe.)\n\
PIPE_CHILD_PID       -  Returns an integer object. (The number corresponds to the process id of the\n\
	                      other side of the link\n\
SOCKET_FD            -  Returns an integer object. (The number corresponds to the socket descriptor\n\
	                      number of the socket.)\n\
SOCKET_PARTNER_ADDR  -  Returns an integer object. (The number corresponds to the integer value of the remote\n\
	                      connections IP number.\n\
SOCKET_PARTNER_PORT  -  Returns an integer object. (The number corresponds to the socket port number on the\n\
	                      remote machine.\n\
\n\
All other values for the selector object will currently return None.";

static char mathlink_FeatureString__doc[] = 
"The featurestring method allows a programmer to retrieve the details of a link that a\n\
link object exchanges with a remote link at connection time.  The method takes no arguments\n\
and returns a string object containing the features of the local link.  Eg:\n\
\n\
MathLink 3 -tokens 33,65,66,67,81,82,83 -formats 1,2,3,7 -decoders 45875";

static char mathlink_Close__doc[] = 
"The close method closes the link object's connection to the remote link.\n\
Depending on the device used by the link, attempts may be made by the device\n\
to deliver as yet undelivered data.";

static char mathlink_SetDefaultYieldFunction__doc[] = 
"The setdefaultyieldfunction will set the yield function for all new link objects.  The function\n\
takes one argument, a callable object, function, or object method.  The programmer should note that\n\
the yield functions for already existing links do not get reset by this call.";

static char mathlink_DefaultYieldFunction__doc[] = 
"The defaultyieldfunction function returns the current value of the mathlink module's default\n\
yield function.  If the module does not have a default yield function, then the function returns\n\
None.";

static char mathlink_GetLink__doc[] = 
"The getlink method allows the programer who has lost track of the original link that\n\
a mark object belongs to, to retrieve the link.  The method takes no arguments and returns\n\
the link object that the mark object marks.";

static char mathlink_Align__doc[] =
"Align";

static char mathlink_Env_Alloc__doc[] = "";

static char mathlink_Alert__doc[] = 
"The alert function activates the alert mechanism currently active in the mathlink module\n\
and displays the alert message.  The function takes one argument, a string object that\n\
contains the text to display by the alert mechanism.  The function returns a long object\n\
holding the exit value of the alert mechanism.";

static char mathlink_Confirm__doc[] = 
"The confirm function question activates the confirm mechanism currently active in the mathlink module\n\
and displays the confirm message.  The function takes two arguments: a string object containing the\n\
text to display by the confirmation mechanism, and an integer object indicating the default response.  The\n\
function returns a long object representing the value of the result returned by the confirmation\n\
mechanism.";

static char mathlink_Request__doc[] = 
"The request function activates the request mechanism currently active in the mathlink module\n\
and displays the request message.  The function takes one argument, a string object containing\n\
the text of the request message for the request mechanism to display.  The function returns a\n\
string object holding the response entered by the user.";

static char mathlink_RequestArgv__doc[] = 
"The requestargv function invokes the current requestargv mechanism installed in the mathlink module.\n\
The mechanism will interactively prompt the user for information necessary to start a link.";

static char mathlink_RequestToInteract__doc[] = 
"The requesttointeract function invokes the currently installed requesttointeract mechanism\n\
installed in the mathlink module.  The function essentially requests the environment for permission\n\
to activate the interactive display mechanisms.  Internally the mathlink module will use requesttointeract\n\
prior to activating the alert, confirm, request, and requesttointeract mechanisms.  The function\n\
takes one argument, an integer argument representing a time to wait.  The function returns\n\
a long object, either 0 or 1, indicating non or permission to interact.";

static char mathlink_SetDialogFunction__doc[] = 
"The setdialogfunction allows the programmer to install a callable object, function or object method\n\
as the code to execute for the mechanisms of alert, confirm, request, requestargv, and requesttointeract.\n\
The function takes two arguments, an integer object holding a selector value and a function/method object.\n\
The function uses the selector argument to indicate which mechanism to set.  The argument can be an actual\n\
integer number or the constants MLAlertFunction, MLConfirmFunction, MLRequestFunction, etc...";

static char mathlink_RequestBufferSize__doc[] =
"The requestbuffersize function returns the current size of the internal buffer used by\n\
the mathlink module to handle interaction for the request mechanism.  The function takes\n\
no arguments.";

static char mathlink_SetRequestBufferSize__doc[] = 
"The setrequestbuffersize allows the python programmer to change the size of the internal\n\
buffer used by the mathlink module to handle interaction via the request,etc... mechanisms.\n\
";

static char mathlink_ArgvSize__doc[] = 
"The argvsize function returns the current maximum size for argv arguments allocated\n\
internally by the mathlink module.";

static char mathlink_SetArgvSize__doc[] = 
"The setargvsize allows the programmer to set the maximum size of the argv data structures used\n\
internally by the mathlink module.";

static char mathlink_ReadyParallel__doc[] =
"The readyparallel function takes a list of link objects and a timeout period.  The function waits on\n\
the link objects in the list and returns the first link object available for reading that receives\n\
incoming data during that period.  The timeout is specified with a tuple containing the seconds and\n\
microseconds to wait: (seconds, microseconds).  For an infinite wait period use (-1, -1).";

/****************** Method assignments ********************/

/* Instance methods table for MLENV objects.  Used by mathlink_getattr() */
static struct PyMethodDef mathlink_Env_methods[] = {
	{"setdefaultyieldfunction", (PyCFunction)mathlink_SetDefaultYieldFunction,   METH_VARARGS, mathlink_SetDefaultYieldFunction__doc},
	{"defaultyieldfunction",    (PyCFunction)mathlink_DefaultYieldFunction,      METH_VARARGS, mathlink_DefaultYieldFunction__doc},
	{"open",                    (PyCFunction)mathlink_Open,                      METH_VARARGS|METH_KEYWORDS, mathlink_Open__doc},
	{"openstring",              (PyCFunction)mathlink_OpenString,                METH_VARARGS, mathlink_OpenString__doc},
	{"openargv",                (PyCFunction)mathlink_OpenArgv,                  METH_VARARGS, mathlink_OpenArgv__doc},
	{"loopbackopen",            (PyCFunction)mathlink_LoopbackOpen,              METH_VARARGS, mathlink_LoopbackOpen__doc},
	{"confirm",                 (PyCFunction)mathlink_Confirm,                   METH_VARARGS, mathlink_Confirm__doc},
	{"alert",                   (PyCFunction)mathlink_Alert,                     METH_VARARGS, mathlink_Alert__doc},
	{"request",                 (PyCFunction)mathlink_Request,                   METH_VARARGS, mathlink_Request__doc},
	{"requestargv",             (PyCFunction)mathlink_RequestArgv,               METH_VARARGS, mathlink_RequestArgv__doc},
	{"requesttointeract",       (PyCFunction)mathlink_RequestToInteract,         METH_VARARGS, mathlink_RequestToInteract__doc},
	{"setdialogfunction",       (PyCFunction)mathlink_SetDialogFunction,         METH_VARARGS, mathlink_SetDialogFunction__doc},
	{"readyparallel",           (PyCFunction)mathlink_ReadyParallel,             METH_VARARGS, mathlink_ReadyParallel__doc},
	{NULL,NULL}
};


/* Instance methods table. Used by mathlink_getattr() */
static struct PyMethodDef mathlink_Link_methods[] = {
	{"clearerror",              (PyCFunction) mathlink_ClearError,                  METH_VARARGS, mathlink_ClearError__doc},
	{"error",                   (PyCFunction) mathlink_Error,                       METH_VARARGS, mathlink_Error__doc},
	{"errorstring",             (PyCFunction) mathlink_ErrorString,                 METH_VARARGS, mathlink_ErrorString__doc},
	{"errorbehavior",           (PyCFunction) mathlink_DefaultErrBehavior,          METH_VARARGS, mathlink_DefaultErrBehavior__doc},
	{"putinteger",              (PyCFunction) mathlink_PutInteger,                  METH_VARARGS, mathlink_PutInteger__doc},
	{"getinteger",              (PyCFunction) mathlink_GetInteger,                  METH_VARARGS, mathlink_GetInteger__doc},
	{"putlong",                 (PyCFunction) mathlink_PutLong,                     METH_VARARGS, mathlink_PutLong__doc},
	{"getlong",                 (PyCFunction) mathlink_GetLong,                     METH_VARARGS, mathlink_GetLong__doc},
	{"putfloat",                (PyCFunction) mathlink_PutFloat,                    METH_VARARGS, mathlink_PutFloat__doc},
	{"getfloat",                (PyCFunction) mathlink_GetFloat,                    METH_VARARGS, mathlink_GetFloat__doc},
	{"putstring",               (PyCFunction) mathlink_PutString,                   METH_VARARGS, mathlink_PutString__doc},
	{"getstring",               (PyCFunction) mathlink_GetString,                   METH_VARARGS, mathlink_GetString__doc},
	{"putbytestring",           (PyCFunction) mathlink_PutByteString,               METH_VARARGS, mathlink_PutByteString__doc},
	{"getbytestring",           (PyCFunction) mathlink_GetByteString,               METH_VARARGS, mathlink_GetByteString__doc},
	{"putucs2string",           (PyCFunction) mathlink_PutUCS2String,               METH_VARARGS, mathlink_PutUCS2String__doc},
	{"getucs2string",           (PyCFunction) mathlink_GetUCS2String,               METH_VARARGS, mathlink_GetUCS2String__doc},
	{"pututf8string",           (PyCFunction) mathlink_PutUTF8String,               METH_VARARGS, mathlink_PutUTF8String__doc},
	{"getutf8string",           (PyCFunction) mathlink_GetUTF8String,               METH_VARARGS, mathlink_GetUTF8String__doc},
	{"pututf16string",          (PyCFunction) mathlink_PutUTF16String,              METH_VARARGS, mathlink_PutUTF16String__doc},
	{"getutf16string",          (PyCFunction) mathlink_GetUTF16String,              METH_VARARGS, mathlink_GetUTF16String__doc},
	{"pututf32string",          (PyCFunction) mathlink_PutUTF32String,              METH_VARARGS, mathlink_PutUTF32String__doc},
	{"getutf32string",          (PyCFunction) mathlink_GetUTF32String,              METH_VARARGS, mathlink_GetUTF32String__doc},
	{"put7bitcharacters",       (PyCFunction) mathlink_Put7BitCharacters,           METH_VARARGS, mathlink_Put7BitCharacters__doc},
	{"putcomplex",              (PyCFunction) mathlink_PutComplex,                  METH_VARARGS, mathlink_PutComplex__doc},
	{"getcomplex",              (PyCFunction) mathlink_GetComplex,                  METH_VARARGS, mathlink_GetComplex__doc},
	{"putnumber",               (PyCFunction) mathlink_PutNumber,                   METH_VARARGS, mathlink_PutNumber__doc},
	{"getnumber",               (PyCFunction) mathlink_GetNumber,                   METH_VARARGS, mathlink_GetNumber__doc},
	{"putsymbol",               (PyCFunction) mathlink_PutSymbol,                   METH_VARARGS, mathlink_PutSymbol__doc},
	{"getsymbol",               (PyCFunction) mathlink_GetSymbol,                   METH_VARARGS, mathlink_GetSymbol__doc},
	{"putunicodesymbol",        (PyCFunction) mathlink_PutUCS2Symbol,               METH_VARARGS, mathlink_PutUCS2Symbol__doc},
	{"getunicodesymbol",        (PyCFunction) mathlink_GetUCS2Symbol,               METH_VARARGS, mathlink_GetUCS2Symbol__doc},
	{"pututf8symbol",           (PyCFunction) mathlink_PutUTF8Symbol,               METH_VARARGS, mathlink_PutUTF8Symbol__doc},
	{"getutf8symbol",           (PyCFunction) mathlink_GetUTF8Symbol,               METH_VARARGS, mathlink_GetUTF8Symbol__doc},
	{"pututf16symbol",          (PyCFunction) mathlink_PutUTF16Symbol,              METH_VARARGS, mathlink_PutUTF16Symbol__doc},
	{"getutf16symbol",          (PyCFunction) mathlink_GetUTF16Symbol,              METH_VARARGS, mathlink_GetUTF16Symbol__doc},
	{"pututf32symbol",          (PyCFunction) mathlink_PutUTF32Symbol,              METH_VARARGS, mathlink_PutUTF32Symbol__doc},
	{"getutf32symbol",          (PyCFunction) mathlink_GetUTF32Symbol,              METH_VARARGS, mathlink_GetUTF32Symbol__doc},
	{"putintegerlist",          (PyCFunction) mathlink_PutIntegerList,              METH_VARARGS, mathlink_PutIntegerList__doc},
	{"getintegerlist",          (PyCFunction) mathlink_GetIntegerList,              METH_VARARGS, mathlink_GetIntegerList__doc},
	{"putfloatlist",            (PyCFunction) mathlink_PutFloatList,                METH_VARARGS, mathlink_PutFloatList__doc},
	{"getfloatlist",            (PyCFunction) mathlink_GetFloatList,                METH_VARARGS, mathlink_GetFloatList__doc},
	{"getintarray",             (PyCFunction) mathlink_GetIntArray,                 METH_VARARGS, mathlink_GetIntArray__doc},
	{"getfloatarray",           (PyCFunction) mathlink_GetFloatArray,               METH_VARARGS, mathlink_GetFloatArray__doc},
	{"putsize",                 (PyCFunction) mathlink_PutSize,                     METH_VARARGS, mathlink_PutSize__doc},
	{"putdata",                 (PyCFunction) mathlink_PutData,                     METH_VARARGS, mathlink_PutData__doc},
	{"getdata",                 (PyCFunction) mathlink_GetData,                     METH_VARARGS, mathlink_GetData__doc},
	{"putrawsize",              (PyCFunction) mathlink_PutRawSize,                  METH_VARARGS, mathlink_PutRawSize__doc},
	{"putrawdata",              (PyCFunction) mathlink_PutRawData,                  METH_VARARGS, mathlink_PutRawData__doc},
	{"getrawdata",              (PyCFunction) mathlink_GetRawData,                  METH_VARARGS, mathlink_GetRawData__doc},
	{"bytestoget",              (PyCFunction) mathlink_BytesToGet,                  METH_VARARGS, mathlink_BytesToGet__doc},
	{"rawbytestoget",           (PyCFunction) mathlink_RawBytesToGet,               METH_VARARGS, mathlink_RawBytesToGet__doc},
	{"bytestoput",              (PyCFunction) mathlink_BytesToPut,                  METH_VARARGS, mathlink_BytesToPut__doc},
	{"newpacket",               (PyCFunction) mathlink_NewPacket,                   METH_VARARGS, mathlink_NewPacket__doc},
	{"nextpacket",              (PyCFunction) mathlink_NextPacket,                  METH_VARARGS, mathlink_NextPacket__doc},
	{"endpacket",               (PyCFunction) mathlink_EndPacket,                   METH_VARARGS, mathlink_EndPacket__doc},
	{"putfunction",             (PyCFunction) mathlink_PutFunction,                 METH_VARARGS, mathlink_PutFunction__doc},
	{"getfunction",             (PyCFunction) mathlink_GetFunction,                 METH_VARARGS, mathlink_GetFunction__doc},
	{"checkfunction",           (PyCFunction) mathlink_CheckFunction,               METH_VARARGS, mathlink_CheckFunction__doc},
	{"puttype",                 (PyCFunction) mathlink_PutType,                     METH_VARARGS, mathlink_PutType__doc},
	{"gettype",                 (PyCFunction) mathlink_GetType,                     METH_VARARGS, mathlink_GetType__doc},
	{"getrawtype",              (PyCFunction) mathlink_GetRawType,                  METH_VARARGS, mathlink_GetRawType__doc},
	{"putnext",                 (PyCFunction) mathlink_PutNext,                     METH_VARARGS, mathlink_PutNext__doc},
	{"getnext",                 (PyCFunction) mathlink_GetNext,                     METH_VARARGS, mathlink_GetNext__doc},
	{"getnextraw",              (PyCFunction) mathlink_GetNextRaw,                  METH_VARARGS, mathlink_GetNextRaw__doc},
	{"putargcount",             (PyCFunction) mathlink_PutArgCount,                 METH_VARARGS, mathlink_PutArgCount__doc},
	{"getargcount",             (PyCFunction) mathlink_GetArgCount,                 METH_VARARGS, mathlink_GetArgCount__doc},
	{"getrawargcount",          (PyCFunction) mathlink_GetRawArgCount,              METH_VARARGS, mathlink_GetRawArgCount__doc},
	{"setyieldfunction",        (PyCFunction) mathlink_SetYieldFunction,            METH_VARARGS, mathlink_SetYieldFunction__doc},
	{"yieldfunction",           (PyCFunction) mathlink_YieldFunction,               METH_VARARGS, mathlink_YieldFunction__doc},
	{"setmessagehandler",       (PyCFunction) mathlink_SetMessageHandler,           METH_VARARGS, mathlink_SetMessageHandler__doc},
	{"messagehandler",          (PyCFunction) mathlink_MessageHandler,              METH_VARARGS, mathlink_MessageHandler__doc},
	{"getmessage",              (PyCFunction) mathlink_GetMessage,                  METH_VARARGS, mathlink_GetMessage__doc},
	{"messageready",            (PyCFunction) mathlink_MessageReady,                METH_VARARGS, mathlink_MessageReady__doc},
	{"putmessage",              (PyCFunction) mathlink_PutMessage,                  METH_VARARGS, mathlink_PutMessage__doc},
	{"createmark",              (PyCFunction) mathlink_CreateMark,                  METH_VARARGS, mathlink_CreateMark__doc},
	{"seektomark",              (PyCFunction) mathlink_SeekToMark,                  METH_VARARGS, mathlink_SeekToMark__doc},
	{"seekmark",                (PyCFunction) mathlink_SeekMark,                    METH_VARARGS, mathlink_SeekMark__doc},
	{"transferexpression",      (PyCFunction) mathlink_TransferExpression,          METH_VARARGS, mathlink_TransferExpression__doc},
	{"ready",                   (PyCFunction) mathlink_Ready,                       METH_VARARGS, mathlink_Ready__doc},
	{"name",                    (PyCFunction) mathlink_GetName,                     METH_VARARGS, mathlink_GetName__doc},
	{"setname",                 (PyCFunction) mathlink_SetName,                     METH_VARARGS, mathlink_SetName__doc},
	{"flush",                   (PyCFunction) mathlink_Flush,                       METH_VARARGS, mathlink_Flush__doc},
	{"duplicate",               (PyCFunction) mathlink_Duplicate,                   METH_VARARGS, mathlink_Duplicate__doc},
	{"connect",                 (PyCFunction) mathlink_Connect,                     METH_VARARGS, mathlink_Connect__doc},
	{"establishstring",         (PyCFunction) mathlink_EstablishString,             METH_VARARGS|METH_KEYWORDS, mathlink_EstablishString__doc},
	{"deviceinformation",       (PyCFunction) mathlink_DeviceInformation,           METH_VARARGS, mathlink_DeviceInformation__doc},
	{"featurestring",           (PyCFunction) mathlink_FeatureString,               METH_VARARGS, mathlink_FeatureString__doc},
	{"close",                   (PyCFunction) mathlink_Close,                       METH_VARARGS, mathlink_Close__doc},
	{NULL,NULL}
};

static struct PyMethodDef mathlink_Mark_methods[] = {
	{"getlink",   (PyCFunction) mathlink_GetLink,    METH_VARARGS, mathlink_GetLink__doc},
	{NULL,NULL}
};


/* Module Methods Table */
static struct PyMethodDef mathlink_methods[] = {
	{"align",                   (PyCFunction)mathlink_Align,                     METH_VARARGS, mathlink_Align__doc},
	{"env",                     (PyCFunction)Env_Alloc,                          METH_VARARGS, mathlink_Env_Alloc__doc},
	{"requestbuffersize",       (PyCFunction)mathlink_RequestBufferSize,         METH_VARARGS, mathlink_RequestBufferSize__doc},
	{"setrequestbuffersize",    (PyCFunction)mathlink_SetRequestBufferSize,      METH_VARARGS, mathlink_SetRequestBufferSize__doc},
	{"argvsize",                (PyCFunction)mathlink_ArgvSize,                  METH_VARARGS, mathlink_ArgvSize__doc},
	{"setargvsize",             (PyCFunction)mathlink_SetArgvSize,               METH_VARARGS, mathlink_SetArgvSize__doc},
	{NULL,NULL}
};


/****************** Function Definitions ********************/

#define CheckForThreadsAndRunEnv(env,expr) \
	if(mathlink_EnvAllowThreadsToRun(env)){ \
		Py_BEGIN_ALLOW_THREADS \
		expr; \
		Py_END_ALLOW_THREADS \
	} \
	else expr

#define CheckForThreadsAndRunLink(link,expr) \
	if(mathlink_LinkAllowThreadsToRun(link)){ \
		Py_BEGIN_ALLOW_THREADS \
		expr; \
		Py_END_ALLOW_THREADS \
	} \
	else expr

static int mathlink_EnvAllowThreadsToRun(mathlink_Env *self)
{
	if(self == (mathlink_Env*)0) return 1;
	
	if(self->DefaultYieldFunction != (PyObject *)0) return 0;
	if(self->ConfirmFunction != (PyObject *)0) return 0;
	if(self->AlertFunction != (PyObject *)0) return 0;
	if(self->RequestFunction != (PyObject *)0) return 0;
	if(self->RequestArgvFunction != (PyObject *)0) return 0;
	if(self->RequestToInteractFunction != (PyObject *)0) return 0;
	
	return 1;
}


static int mathlink_LinkAllowThreadsToRun(mathlink_Link *self)
{
	mathlink_Env *env;
	
	if(self == (mathlink_Link *)0) return 1;
	
	env = self->PyEnv;
	if(! mathlink_EnvAllowThreadsToRun(env)) return 0;
	if(self->yieldfunction.func != (PyObject *)0) return 0;
	if(self->messagehandler.func != (PyObject *)0) return 0;
	
	return 1;
}



/* if switch == 1, then we want a[code] = string
   if switch == 0, then we want a[string] = code
*/
void mathlink_AddMessageCodesToDict(PyObject *Dict, message_code *message,int swtch)
{
	PyObject *Int, *String;
	int result = 2;
	  
	/* Populate the Dictionary */
	while(message->mes != (char *)0){
		/* Convert the err to a python Int */
		Int = PyInt_FromLong((long)message->code);
		if(Int == NULL) return;
		String = PyString_FromString(message->mes);
		if(String == NULL) return;
		if(swtch == 1) result = PyDict_SetItem(Dict, Int, String);
		else if(swtch == 0) result = PyDict_SetItem(Dict, String, Int);
		if(result < 0) return;
		Py_DECREF(Int);
		Py_DECREF(String);
		message++;
	}
}

/* link object Instance Methods */

void mathlink_SetErrorConditionFromLink(mathlink_Link *Link)
{
	PyObject *key, *value, *string;
	long mlerror, mlcresult;
	kcharp_ct err_string;

	mlerror = MLError(Link->lp);

	err_string = MLErrorString(Link->PyEnv->env, mlerror);

	if(err_string == (kcharp_ct)0){
		key = PyInt_FromLong((long)mlerror);
		value = PyDict_GetItem(ErrorDictionary, key);
		PyErr_SetObject(mathlinkError,value);
		Py_DECREF(key);
	}
	else{
		string = PyString_FromString(err_string);
		PyErr_SetObject(mathlinkError,string);
	}

	if(Link->autoclear) mlcresult = MLClearError(Link->lp);
}

void mathlink_SetErrorConditionFromCode(int code)
{
	PyObject *key, *value;

	key = PyInt_FromLong((long)code);
	value = PyDict_GetItem(ErrorDictionary, key);
	PyErr_SetObject(mathlinkError,value);
	Py_DECREF(key);
}

#define EQUAL    0 
#define NOTEQUAL 1

int checkforerror(long result, long expected, mathlink_Link *self,int selector)
{

	switch(selector){
		case EQUAL:
			if(result == expected){
				mathlink_SetErrorConditionFromLink(self);
				return 0;
			}
			break;
		case NOTEQUAL:
			if(result != expected){
				mathlink_SetErrorConditionFromLink(self);
				return 0;
			}
			break;
	}
	return 1;
}

#define CHECKEQUAL(result,expect,link) if(! checkforerror(result,expect,link,EQUAL)) return NULL
#define CHECKNOTEQUAL(result,expect,link) if(! checkforerror(result,expect,link,NOTEQUAL)) return NULL

static PyObject * mathlink_ClearError(mathlink_Link *self, PyObject *args)
{
	int result;
	 
	if(PyTuple_Size(args) > 0) return NULL;
	
	result = MLClearError(self->lp);
	
	if(result != MLSUCCESS){
		PyErr_SetString(mathlinkError, "Failure to Clear mathlink Error condition");
		return NULL;
	}
	
	/* Return Py_None */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_Error(mathlink_Link *self, PyObject *args)
{
	int result;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	result = MLError(self->lp);
	
	return PyInt_FromLong((long)result);
}


static PyObject * mathlink_ErrorString(mathlink_Link *self, PyObject *args)
{
	PyObject *string;
	long mlerror;
	kcharp_ct err_string;

	if(PyTuple_Size(args) > 0) return NULL;
	
	mlerror =  MLError(self->lp);
	
	err_string = MLErrorString(self->PyEnv->env, mlerror);
	
	if(err_string == (kcharp_ct)0){
		string = PyString_FromString("Unrecognized error");
	}
	else{
		string = PyString_FromString(err_string);
	}
	
	return string;
}

static PyObject * mathlink_DefaultErrBehavior(mathlink_Link *self, PyObject *args)
{
	int autoclear;
	
	if(! PyArg_ParseTuple(args, "i", &autoclear)) return NULL;
	
	if(autoclear > 0) self->autoclear = 1;
	else if(autoclear <= 0) self->autoclear = 0;
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_PutInteger(mathlink_Link *self, PyObject *args)
{
	
	int data, result;
	 
	/* We parse the args for the Integer... */
	if(!PyArg_ParseTuple(args,"i",&data)) return NULL;
	 
	CheckForThreadsAndRunLink(self,result = MLPutInteger(self->lp, data));	
	CHECKNOTEQUAL(result,MLSUCCESS,self);

	/* Return None */   
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetInteger(mathlink_Link *self, PyObject *args)
{

	int data, result;
	 
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLGetInteger(self->lp, &data));
	CHECKNOTEQUAL(result,MLSUCCESS,self);

	return Py_BuildValue("i",data);
}


static PyObject * mathlink_PutLong(mathlink_Link *self, PyObject *args)
{
	int result;
	PyObject *Long, *LString;
	char *lstring;

	if(! PyTuple_Check(args)) return NULL; 				/* Perhaps we need an exception here... */
	if(PyTuple_Size(args) > 1) return NULL; 				/* We definitely need an exception here for type stuff... */
	  
	if((Long = PyTuple_GetItem(args,0)) == NULL) return NULL; 		/* Here we need another exception */
	
	/* Long should now have the Long data object, we need to convert it to a C type... */
	/* Check to make sure we have a Long...*/
	if((! PyLong_Check(Long)) && (! PyInt_Check(Long))){
		PyErr_SetString(PyExc_TypeError, "Argument not a type Int or Long");
		return NULL;
	}			
	  
	/* Now convert Python Long to a Python string */
	if((LString = PyObject_Str(Long)) == NULL) return NULL; 		/* Another exception */
	  
	/* Now convert the Python String to a C string. */
	if((lstring = PyString_AsString(LString)) == NULL) return NULL; 	/* Another Exception */
	
	/* Send the Int token type... */
	result = MLPutNext(self->lp,MLTKINT);

	CHECKNOTEQUAL(result,MLSUCCESS,self);
	  
	/* Now use MLPutByteString to send the number */
	CheckForThreadsAndRunLink(self,result = MLPutByteString(self->lp,(unsigned char *)lstring,(long)strlen(lstring)));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	 
	Py_DECREF(LString);
	 
	/* Return None */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetLong(mathlink_Link *self, PyObject *args)
{
	PyObject *Long, *LString;
	unsigned char *lstring;
	char *mystring;
	int result,len;
	
	/* First Check args... */
	if(PyTuple_Size(args) > 0) return NULL; /* exception */
	
	/* Now get the data from mathlink... */
	result = MLGetType(self->lp);

	if(result == MLTKERROR){
		mathlink_SetErrorConditionFromLink(self);
		return NULL;
	}

 	if(result != MLTKINT){
		PyErr_SetString(mathlinkError, "Got unexpected token type from link.  Incoming type is not an Integer.");
		return NULL;
	}

	CheckForThreadsAndRunLink(self,result = MLGetByteString(self->lp,(const unsigned char **)&lstring,&len,0));
	CHECKNOTEQUAL(result,MLSUCCESS,self);

	/* Use some Python memory */
	mystring = PyMem_New(char, len + 1);
	memset((void *)mystring, 0, len + 1);
	if(mystring == (char *)0){
		PyErr_SetString(mathlinkError,"Failed to allocate memory in order to get a Long number from the link");
		return NULL;
	}
	
	memcpy(mystring,lstring,len);
	
	/* Convert the string to a Python String... */
	if((LString = PyString_FromString(mystring)) == NULL) return NULL;           /* exception */
	
	/* Convert the Python String to a Python Long... */
	if((Long = PyNumber_Long(LString)) == NULL) return NULL;                    /* exception */
 
	/* Clean up the Python and mathlink memory use */
	Py_DECREF(LString);
	MLDisownByteString(self->lp,(unsigned char *) lstring, len);
	PyMem_Free((void * )mystring);
	  
	/* Now return the Long ... */
	return Long;
}


static PyObject * mathlink_PutFloat(mathlink_Link *self, PyObject *args)
{
	double data;
	int result;
	
	/* Get the argument from the args... */
	if(!PyArg_ParseTuple(args,"d",&data)) return NULL;                /* exception */

	/* Now send the argument via mathlink... */
	CheckForThreadsAndRunLink(self,result = MLPutDouble(self->lp,data));
	CHECKNOTEQUAL(result,MLSUCCESS,self);

	/* Return None */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetFloat(mathlink_Link *self, PyObject *args)
{
	double data;
	int result;
	PyObject *Float;
	
	/* Check Args... */
	if(PyTuple_Size(args) > 0) return NULL;                        /* exception */
	
	/* Get the data from mathlink... */
	CheckForThreadsAndRunLink(self,result = MLGetDouble(self->lp,&data));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	  
	/* Convert the C float to a Python Float... */
	if((Float = PyFloat_FromDouble(data)) == NULL) return NULL;        /* exception */
	
	/* Return the new Float */
	return Float;
}


static PyObject * mathlink_PutString(mathlink_Link *self, PyObject *args)
{
	char *string;
	int result;
	
	/* Get the argument from args... */
	if(!PyArg_ParseTuple(args,"s",&string)) return NULL;                /* exception */
	
	/* Send the data across mathlink... */
	CheckForThreadsAndRunLink(self,result = MLPutString(self->lp,string));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	/* Return None */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetString(mathlink_Link *self, PyObject *args)
{
	const char *string;
	int result;
	PyObject *String;
	
	/* Check to make sure we have no args... */
	if(PyTuple_Size(args) > 0) return NULL;                       /* exception */
	
	/* get the string from mathlink... */
	CheckForThreadsAndRunLink(self,result = MLGetString(self->lp,&string));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	/* Now create a Python String... */
	if((String = PyString_FromString(string)) == NULL) return NULL;     /* exception */
	
	/* Clean up the memory usage from mathlink... */
	MLDisownString(self->lp,string);
	
	return String;
}


static PyObject * mathlink_PutByteString(mathlink_Link *self, PyObject *args)
{
	char	*s;
	long	len, result;

	if(!PyArg_ParseTuple(args, "s#", &s, &len)) return NULL;

	CheckForThreadsAndRunLink(self,result = MLPutByteString(self->lp, (const unsigned char *)s, len));
	CHECKNOTEQUAL(result, MLSUCCESS, self);

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetByteString(mathlink_Link *self, PyObject *args)
{
	char            *s;
	long            spec = 0;
	int             len, result;
	PyObject        *obj;

	if(!PyArg_ParseTuple(args, "|i", &spec)) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLGetByteString(self->lp, (const unsigned char**) &s, &len, spec));
	CHECKNOTEQUAL(result, MLSUCCESS, self);
		
	obj = PyString_FromStringAndSize(s, len);
	
	MLDisownByteString(self->lp, (const unsigned char *)s, len);
	
	return obj;
}


static PyObject * mathlink_PutUCS2String(mathlink_Link *self, PyObject *args)
{
	Py_UNICODE	*s;
	long		len, result;
	
	if(!PyArg_ParseTuple(args, "u#", &s, &len)) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLPutUCS2String(self->lp, (const unsigned short *)s, (int)len));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetUCS2String(mathlink_Link *self, PyObject *args)
{
	PyObject	*obj;
	Py_UNICODE	*s;
	long		result;
	int		len;

	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLGetUCS2String(self->lp, (const unsigned short **)&s, (int *)&len));
	CHECKNOTEQUAL(result,MLSUCCESS,self);

	obj = PyUnicode_FromUnicode(s, len);
	
	MLReleaseUCS2String(self->lp, (const unsigned short *)s, len);
	
	return obj;
}


static PyObject * mathlink_PutUTF8String(mathlink_Link *self, PyObject *args)
{
	char	*s;
	long	len, result;

	if(!PyArg_ParseTuple(args, "s#", &s, &len)) return NULL;

	CheckForThreadsAndRunLink(self,result = MLPutUTF8String(self->lp, (const unsigned char *)s, (int)len));
	CHECKNOTEQUAL(result, MLSUCCESS, self);

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetUTF8String(mathlink_Link *self, PyObject *args)
{
	unsigned char	*s;
	long		result;
	int		nbytes, nchars;
	PyObject	*obj;

	if(PyTuple_Size(args) > 0) return NULL;

	CheckForThreadsAndRunLink(self,result = MLGetUTF8String(self->lp, (const unsigned char**) &s, &nbytes, &nchars));
	CHECKNOTEQUAL(result, MLSUCCESS, self);
		
	obj = PyString_FromStringAndSize((char *)s, nbytes);
	
	MLReleaseUTF8String(self->lp, (const unsigned char *)s, nbytes);
	
	return obj;
}


static PyObject * mathlink_PutUTF16String(mathlink_Link *self, PyObject *args)
{
	char *s;
	long len, result;

	if(!PyArg_ParseTuple(args, "s#", &s, &len)) return NULL;

	CheckForThreadsAndRunLink(self,result = MLPutUTF16String(self->lp, (const unsigned short *)s, (int)(len / 2)));
	CHECKNOTEQUAL(result, MLSUCCESS, self);

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetUTF16String(mathlink_Link *self, PyObject *args)
{
	unsigned char	*s;
	long		result;
	int		ncodes, nchars;
	PyObject	*obj;

	if(PyTuple_Size(args) > 0) return NULL;

	CheckForThreadsAndRunLink(self,result = MLGetUTF16String(self->lp, (const unsigned short**) &s, &ncodes, &nchars));
	CHECKNOTEQUAL(result, MLSUCCESS, self);
		
	obj = PyString_FromStringAndSize((char *)s, ncodes * 2);
	
	MLReleaseUTF16String(self->lp, (const unsigned short *)s, ncodes);
	
	return obj;
}


static PyObject * mathlink_PutUTF32String(mathlink_Link *self, PyObject *args)
{
	char *s;
	long len, result;

	if(!PyArg_ParseTuple(args, "s#", &s, &len)) return NULL;

	CheckForThreadsAndRunLink(self,result = MLPutUTF32String(self->lp, (const unsigned int *)s, (int)(len / 4)));
	CHECKNOTEQUAL(result, MLSUCCESS, self);

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetUTF32String(mathlink_Link *self, PyObject *args)
{
	unsigned char	*s;
	long		result;
	int		len;
	PyObject	*obj;

	if(PyTuple_Size(args) > 0) return NULL;

	CheckForThreadsAndRunLink(self,result = MLGetUTF32String(self->lp, (const unsigned int**) &s, &len));
	CHECKNOTEQUAL(result, MLSUCCESS, self);

	obj = PyString_FromStringAndSize((char *)s, len * 4);

	MLReleaseUTF32String(self->lp, (const unsigned int *)s, len);

	return obj;
}


static PyObject * mathlink_Put7BitCharacters(mathlink_Link *self, PyObject *args)
{
	char *string;
	int result, length, remaining;
	
	/* Get the argument from args... */
	if(!PyArg_ParseTuple(args,"s#i",&string, &length, &remaining)) return NULL;                /* exception */
	
	/* Send the data across mathlink... */
	CheckForThreadsAndRunLink(self,result = MLPut7BitCharacters(self->lp, remaining, (const char *)string, length, length));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	/* Return None */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_PutComplex(mathlink_Link *self, PyObject *args)
{
	Py_complex complex;
	int result;
	
	if(! PyArg_ParseTuple(args,"D",&complex)) return NULL;
		
	/* Send HEAD across the link... */
	CheckForThreadsAndRunLink(self,result = MLPutFunction(self->lp,"Complex",2));
	CHECKNOTEQUAL(result,MLSUCCESS,self);

	/* Send the Data, real first, then imaginary... */
	CheckForThreadsAndRunLink(self,result = MLPutDouble(self->lp,complex.real));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	CheckForThreadsAndRunLink(self,result = MLPutDouble(self->lp,complex.imag));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetComplex(mathlink_Link *self, PyObject *args)
{
	Py_complex complex;
	int result, argument;
	char *function;
	
	if(PyTuple_Size(args) > 0) return NULL;


	/* Read the Packet head from the link... */
	CheckForThreadsAndRunLink(self,result = MLGetFunction(self->lp,(const char **)&function, &argument));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	if(strstr(function,"Complex") == NULL){
		PyErr_SetString(mathlinkError,"Unexpected function head encountered on link, failed to get Complex number");
		return NULL;
	}
	if(argument != 2){
		PyErr_SetString(mathlinkError,"Unexpected function length encountered on link, failed to get Complex number");
		return NULL;
	}
	
	/* Now get the numbers, Real first, then Imaginary... */
	CheckForThreadsAndRunLink(self,result = MLGetDouble(self->lp,&complex.real));
	CHECKNOTEQUAL(result,MLSUCCESS,self);

	CheckForThreadsAndRunLink(self,result = MLGetDouble(self->lp,&complex.imag));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	return PyComplex_FromCComplex(complex);
}


static PyObject * mathlink_PutNumber(mathlink_Link *self, PyObject *args)
{
	PyObject *Number, *Result;
	long result;
	
	Number = PyTuple_GetItem(args,0);
	if(Number == NULL) return NULL;

	if(PyInt_Check(Number)){ Result = mathlink_PutInteger(self,args); }
	else if(PyLong_Check(Number)){ Result = mathlink_PutLong(self,args); }
	else if(PyFloat_Check(Number)){ Result = mathlink_PutFloat(self,args); }
	else if(PyComplex_Check(Number)){ Result = mathlink_PutComplex(self,args); }
	else{
		PyErr_SetString(PyExc_TypeError, "Argument is not an Int, Long, Float, or Complex object");
		return NULL;
	}
	
	Py_DECREF(Result); /* This should be Py_None... */
	
	/* Now we send MLFlush... */
	result = MLFlush(self->lp);
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetNumber(mathlink_Link *self, PyObject *args)
{
	long result;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLGetType(self->lp));

	CHECKEQUAL(result,MLTKERROR,self);
	
	if(result == MLTKINT) return mathlink_GetLong(self,args);              /* By default we will always return a Long... */
	else if(result == MLTKREAL) return mathlink_GetFloat(self,args);       /* Call mathlink_GetFloat... */
	else if(result == MLTKFUNC) return mathlink_GetComplex(self,args);    /* We will look for Complex... */
	
	Py_INCREF(Py_None);
	return Py_None;
}
	    

static PyObject * mathlink_PutSymbol(mathlink_Link *self, PyObject *args)
{
	char *symbol;
	long result;
	
	if(!PyArg_ParseTuple(args,"s",&symbol)) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLPutSymbol(self->lp,symbol));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetSymbol(mathlink_Link *self, PyObject *args)
{
	const char *symbol;
	long result;
	PyObject *String;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLGetSymbol(self->lp, (const char **)&symbol));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	String = PyString_FromString(symbol);
	if(String == NULL) return NULL;
 
	MLDisownSymbol(self->lp,symbol);
	
	return String;
}


static PyObject * mathlink_PutUCS2Symbol(mathlink_Link *self, PyObject *args)
{
	Py_UNICODE	*s;
	long		len, result;
	
	if(!PyArg_ParseTuple(args, "u#", &s, &len)) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLPutUCS2Symbol(self->lp, (const unsigned short *)s, (int)len));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetUCS2Symbol(mathlink_Link *self, PyObject *args)
{
	PyObject	*obj;
	Py_UNICODE	*s;
	long		result;
	int		len;

	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLGetUCS2Symbol(self->lp, (const unsigned short **)&s, &len));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	obj = PyUnicode_FromUnicode(s, len);
	
	MLReleaseUCS2Symbol(self->lp, (const unsigned short *)s, len);
	
	return obj;
}


static PyObject * mathlink_PutUTF8Symbol(mathlink_Link *self, PyObject *args)
{
	char	*s;
	long	len, result;

	if(!PyArg_ParseTuple(args, "s#", &s, &len)) return NULL;

	CheckForThreadsAndRunLink(self,result = MLPutUTF8Symbol(self->lp, (const unsigned char *)s, (int)len));
	CHECKNOTEQUAL(result, MLSUCCESS, self);

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetUTF8Symbol(mathlink_Link *self, PyObject *args)
{
	unsigned char	*s;
	long		result;
	int		nbytes, nchars;
	PyObject	*obj;

	if(PyTuple_Size(args) > 0) return NULL;

	CheckForThreadsAndRunLink(self,result = MLGetUTF8Symbol(self->lp, (const unsigned char**) &s, &nbytes, &nchars));
	CHECKNOTEQUAL(result, MLSUCCESS, self);
		
	obj = PyString_FromStringAndSize((char *)s, nbytes);
	
	MLReleaseUTF8Symbol(self->lp, (const unsigned char *)s, nbytes);
	
	return obj;
}


static PyObject * mathlink_PutUTF16Symbol(mathlink_Link *self, PyObject *args)
{
	char *s;
	long len, result;

	if(!PyArg_ParseTuple(args, "s#", &s, &len)) return NULL;

	CheckForThreadsAndRunLink(self,result = MLPutUTF16Symbol(self->lp, (const unsigned short *)s, (int)(len / 2)));
	CHECKNOTEQUAL(result, MLSUCCESS, self);

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetUTF16Symbol(mathlink_Link *self, PyObject *args)
{
	unsigned char	*s;
	long		result;
	int		ncodes, nchars;
	PyObject	*obj;

	if(PyTuple_Size(args) > 0) return NULL;

	CheckForThreadsAndRunLink(self,result = MLGetUTF16Symbol(self->lp, (const unsigned short**) &s, &ncodes, &nchars));
	CHECKNOTEQUAL(result, MLSUCCESS, self);
		
	obj = PyString_FromStringAndSize((char *)s, ncodes * 2);
	
	MLReleaseUTF16Symbol(self->lp, (const unsigned short *)s, ncodes);
	
	return obj;
}


static PyObject * mathlink_PutUTF32Symbol(mathlink_Link *self, PyObject *args)
{
	char *s;
	long len, result;

	if(!PyArg_ParseTuple(args, "s#", &s, &len)) return NULL;

	CheckForThreadsAndRunLink(self,result = MLPutUTF32Symbol(self->lp, (const unsigned int *)s, (int)(len / 4)));
	CHECKNOTEQUAL(result, MLSUCCESS, self);

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetUTF32Symbol(mathlink_Link *self, PyObject *args)
{
	unsigned char	*s;
	long		result;
	int		len;
	PyObject	*obj;

	if(PyTuple_Size(args) > 0) return NULL;

	CheckForThreadsAndRunLink(self,result = MLGetUTF32Symbol(self->lp, (const unsigned int**) &s, &len));
	CHECKNOTEQUAL(result, MLSUCCESS, self);

	obj = PyString_FromStringAndSize((char *)s, len * 4);

	MLReleaseUTF32Symbol(self->lp, (const unsigned int *)s, len);

	return obj;
}


static PyObject * mathlink_PutIntegerList(mathlink_Link *self, PyObject *args)
{
	PyObject*	seq;
	PyObject*	obj;
	long		i, len, result;
	int*		list;
	
	if(!PyArg_ParseTuple(args, "O", &seq)) return NULL;
	
	if(!PySequence_Check(seq))
	{
		PyErr_SetString(PyExc_TypeError, "Argument is not a sequence object");
		return NULL;
	}
		
	len = PyObject_Length(seq);
	
	list = PyMem_New(int, len);
	for(i = 0; i < len; i++)
	{
		obj = PySequence_GetItem(seq, i);
		if(!PyInt_Check(obj))
		{
			PyMem_Free(list);
			PyErr_SetString(PyExc_TypeError, "Sequence item is not an integer");
			return NULL;
		}
		list[i] = (int)PyInt_AsLong(obj);
	}

	CheckForThreadsAndRunLink(self,result = MLPutIntegerList(self->lp, list, len));

	PyMem_Free(list);
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetIntegerList(mathlink_Link *self, PyObject *args)
{
	PyObject*	tmp;
	PyObject*	seq;
	PyObject*	obj;
	int*		list;
	long		err, len, i;
	
	if(PyTuple_Size(args) > 0) return NULL;

	CheckForThreadsAndRunLink(self,err = MLGetIntegerList(self->lp, &list, &len));
	CHECKNOTEQUAL(err,MLSUCCESS,self);
	
	seq = Py_BuildValue("[]");
	for(i = 0; i < len; i++)
	{
		obj = Py_BuildValue("[i]", list[i]);
		tmp = PySequence_Concat(seq, obj);
		Py_XDECREF(obj);
		Py_XDECREF(seq);
		seq = tmp;
	}
	
	MLDisownIntegerList(self->lp, list, len);
	
	return seq;
}


static PyObject * mathlink_PutFloatList(mathlink_Link *self, PyObject *args)
{
	PyObject	*seq, *obj;
	long		i, len, result;
	double		*list;
	
	if(!PyArg_ParseTuple(args, "O", &seq)) return NULL;
	
	if(!PySequence_Check(seq))
	{
		PyErr_SetString(PyExc_TypeError, "Argument is not a sequence object");
		return NULL;
	}
		
	len = PyObject_Length(seq);
	
	list = PyMem_New(double, len);
	for(i = 0; i < len; i++)
	{
		obj = PySequence_GetItem(seq, i);
		if(!PyFloat_Check(obj))
		{
			if(!PyInt_Check(obj))
			{
				PyMem_Free(list);
				PyErr_SetString(PyExc_TypeError, "Sequence item is not a real");
				return NULL;
			}
			list[i] = (double) PyInt_AsLong(obj);
		}
		else
			list[i] = PyFloat_AsDouble(obj);
			
	}
	
	CheckForThreadsAndRunLink(self,result = MLPutRealList(self->lp, list, len));
	PyMem_Free(list);
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetFloatList(mathlink_Link* self, PyObject *args)
{
	PyObject*	tmp;
	PyObject*	seq;
	PyObject*	obj;
	double*		list;
	long		err, len, i;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,err = MLGetRealList(self->lp, &list, &len));
	CHECKNOTEQUAL(err,MLSUCCESS,self);
	
	seq = Py_BuildValue("[]");
	for(i = 0; i < len; i++)
	{
		obj = Py_BuildValue("[d]", list[i]);
		tmp = PySequence_Concat(seq, obj);
		Py_XDECREF(obj);
		Py_XDECREF(seq);
		seq = tmp;
	}
	
	MLDisownRealList(self->lp, list, len);
	
	return seq;
}


/* 
 * this is a utility function used by GetIntArray & GetFloatArray
 * which recursively creates python lists for each row in a 
 * multidimensional array.
 */
static PyObject* build_row(void* array, int* offset, long* dimensions, int dim, int depth, int isReal)
{
	PyObject*	row;
	PyObject*	obj;
	PyObject*	tmp;
	int			i;
	
	row = Py_BuildValue("[]");

	if(dim == depth - 1)
	{
		for(i = 0; i < dimensions[dim]; i++)
		{
			if(isReal)
				obj = Py_BuildValue("[d]", ((double*) array)[*offset]);
			else
				obj = Py_BuildValue("[i]", ((int*) array)[*offset]);
			
			tmp = PySequence_Concat(row, obj);
			Py_XDECREF(obj);
			Py_XDECREF(row);
			row = tmp;
			
			*offset = *offset + 1;
		}
	}
	else
	{
		for(i = 0; i < dimensions[dim]; i++)
		{
			obj = build_row(array, offset, dimensions, dim+1, depth, isReal);
			tmp = Py_BuildValue("[O]", obj);
			Py_XDECREF(obj);
			obj = tmp;
			
			tmp = PySequence_Concat(row, obj);
			Py_XDECREF(obj);
			Py_XDECREF(row);
			row = tmp;
		}
	}
	return row;
}


static PyObject * mathlink_GetIntArray(mathlink_Link *self, PyObject *args)
{
	PyObject*	seq;
	int*		array;
	long*		dimensions;
	long		depth, err;
	int		offset = 0;
	char**		heads;

	if(PyTuple_Size(args) > 0) return NULL;

	CheckForThreadsAndRunLink(self,err = MLGetIntegerArray(self->lp, &array, &dimensions, &heads, &depth));
	CHECKNOTEQUAL(err,MLSUCCESS,self);

	seq = build_row(array, &offset, dimensions, 0, (int)depth, /* integers */ 0);
	MLDisownIntegerArray(self->lp, array, dimensions, heads, depth);
	
	return seq;
}


static PyObject * mathlink_GetFloatArray(mathlink_Link *self, PyObject *args)
{
	PyObject*	seq;
	double*		array;
	long*		dimensions;
	long		depth, err;
	int			offset = 0;
	char**		heads;

	if(PyTuple_Size(args) > 0) return NULL;

	CheckForThreadsAndRunLink(self,err = MLGetRealArray(self->lp, &array, &dimensions, &heads, &depth));
	CHECKNOTEQUAL(err,MLSUCCESS,self);

	seq = build_row(array, &offset, dimensions, 0, (int)depth, /* reals */ 1);
	MLDisownRealArray(self->lp, array, dimensions, heads, depth);
	
	return seq;
}


static PyObject * mathlink_PutSize(mathlink_Link *self, PyObject *args)
{
	long result, size;
	
	if(! PyArg_ParseTuple(args,"l",&size)) return NULL;

	CheckForThreadsAndRunLink(self,result = MLPutSize(self->lp,(int)size));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	/* Return None */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_PutData(mathlink_Link *self, PyObject *args)
{
	char *string;
	int result, len;
	
	/* Get the argument from args... */
	if(!PyArg_ParseTuple(args,"s#",&string, &len)) return NULL;                /* exception */
	
	/* Send the data across mathlink... */
	CheckForThreadsAndRunLink(self,result = MLPutData(self->lp, string, len));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	/* Return None */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetData(mathlink_Link *self, PyObject *args)
{
	char *string;
	int result;
	int len, got;
	PyObject *String, *Got, *Tuple;
	
	if(!PyArg_ParseTuple(args, "i", &len)) return NULL;
	
	string = PyMem_New(char,len + 1);
	memset((void *)string, 0,len + 1);
	if(string == (char *)0) return NULL;
	
	/* get the string from mathlink... */
	CheckForThreadsAndRunLink(self,result = MLGetData(self->lp,string, len, &got));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	/* Now create a Python String... */
	if((String = PyString_FromString(string)) == NULL) return NULL;     /* exception */
	
	/* Clean up the memory usage from mathlink... */
	PyMem_Free(string);
	
	/* Create the Got value */
	if((Got = PyLong_FromLong(got)) == NULL) return NULL;
	
	if((Tuple = PyTuple_New(2)) == NULL) return NULL;
	
	if(PyTuple_SetItem(Tuple, 0, String) != 0) return NULL;
	if(PyTuple_SetItem(Tuple, 1, Got) != 0) return NULL;
	  
	return Tuple;
}


static PyObject * mathlink_PutRawSize(mathlink_Link *self, PyObject *args)
{
	long result, size;
	
	if(!PyArg_ParseTuple(args,"l",&size)) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLPutRawSize(self->lp,(int)size));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	/* Return None */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_PutRawData(mathlink_Link *self, PyObject *args)
{
	char *string;
	int result, len;
	
	/* Get the argument from args... */
	if(!PyArg_ParseTuple(args,"s#",&string, &len)) return NULL;                /* exception */
	
	/* Send the data across mathlink... */
	CheckForThreadsAndRunLink(self,result = MLPutRawData(self->lp,(unsigned char *)string, len));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	/* Return None */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetRawData(mathlink_Link *self, PyObject *args)
{
	char *string;
	int result;
	int len, got;
	PyObject *String, *Got, *Tuple;
	
	if(!PyArg_ParseTuple(args, "i", &len)) return NULL;
	
	string = PyMem_New(char,len + 1);
	memset((void *)string, 0, len + 1);
	if(string == (char *)0) return NULL;
	
	/* get the string from mathlink... */
	CheckForThreadsAndRunLink(self,result = MLGetRawData(self->lp,(unsigned char *)string, len, &got));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	/* Now create a Python String... */
	if((String = PyString_FromString(string)) == NULL) return NULL;     /* exception */
	
	/* Clean up the memory usage from mathlink... */
	PyMem_Free(string);
	
	/* Create the Got value */
	if((Got = PyLong_FromLong(got)) == NULL) return NULL;
	
	if((Tuple = PyTuple_New(2)) == NULL) return NULL;
	
	if(PyTuple_SetItem(Tuple, 0, String) != 0) return NULL;
	if(PyTuple_SetItem(Tuple, 1, Got) != 0) return NULL;
	  
	return Tuple;
}


static PyObject * mathlink_BytesToGet(mathlink_Link *self, PyObject *args)
{
	long result;
	int left;
	PyObject *Long;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLBytesToGet(self->lp,&left));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	if((Long = PyLong_FromLong(left)) == NULL) return NULL;
	
	return Long;
}


static PyObject * mathlink_RawBytesToGet(mathlink_Link *self, PyObject *args)
{
	long result;
	int left;
	PyObject *Long;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLRawBytesToGet(self->lp,&left));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	if((Long = PyLong_FromLong(left)) == NULL) return NULL;
	
	return Long;
}


static PyObject * mathlink_BytesToPut(mathlink_Link *self, PyObject *args)
{
	long result;
	int left;
	PyObject *Long;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLBytesToPut(self->lp,&left));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	if((Long = PyLong_FromLong(left)) == NULL) return NULL;
	
	return Long;
}


static PyObject * mathlink_NewPacket(mathlink_Link *self, PyObject *args)
{
	long result;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLNewPacket(self->lp));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_EndPacket(mathlink_Link *self, PyObject *args)
{
	long result;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLEndPacket(self->lp));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


/* Its debatable as to whether or not this function should return a string or an
integer indicating the code.  The string would be the packet name -> easily retrieved
from mathlink_PacketDictionary.  Otherwise the user has to do the hard work... 

I will leave it as returning the code for now, but we shall see... */
static PyObject * mathlink_NextPacket(mathlink_Link *self, PyObject *args)
{
	int packet;
	PyObject *Int;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,packet = MLNextPacket(self->lp));
	CHECKEQUAL((long)packet,(long)ILLEGALPKT,self);
	
	Int = PyInt_FromLong((long)packet);
	if(Int == NULL) return NULL;
	
	return Int;
}


static PyObject * mathlink_PutFunction(mathlink_Link *self, PyObject *args)
{
	char *function;
	int arguments;
	long result;
	
	if(! PyArg_ParseTuple(args, "si", &function, &arguments)) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLPutFunction(self->lp,(const char*) function, arguments));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetFunction(mathlink_Link *self, PyObject *args)
{
	char *function;
	int arguments;
	long result;
	PyObject *Tuple;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLGetFunction(self->lp, (const char**) &function, &arguments));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Tuple = Py_BuildValue("(si)",function,arguments);
	MLDisownSymbol(self->lp,function);
	
	return Tuple;
}


static PyObject * mathlink_CheckFunction(mathlink_Link *self, PyObject *args)
{
	char *string;
	int len; 
	long result;
	PyObject *Tuple;
	
	if(! PyArg_ParseTuple(args, "s", &string)) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLCheckFunction(self->lp, (const char *)string, (long *)&len));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Tuple = Py_BuildValue("(ii)",result,len);
	
	return Tuple;
}


static PyObject * mathlink_PutType(mathlink_Link *self, PyObject *args)
{
	long result;
	long token;
	
	if(! PyArg_ParseTuple(args,"l",&token)) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLPutType(self->lp, (int)token));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_GetType(mathlink_Link *self, PyObject *args)
{
	long token;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,token = MLGetType(self->lp));
	CHECKEQUAL(token,MLTKERROR,self);
	
	return Py_BuildValue("l",token);
}
	

static PyObject * mathlink_GetRawType(mathlink_Link *self, PyObject *args)
{
	int result;
	PyObject *Type;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLGetRawType(self->lp));
	CHECKEQUAL((long)result,(long)MLTKERROR,self);
	
	Type = PyInt_FromLong((long) result);
	if(Type == NULL) return NULL;
	
	return Type;
}


static PyObject * mathlink_PutNext(mathlink_Link *self, PyObject *args)
{
	long token, result;
	
	if(! PyArg_ParseTuple(args,"l",&token)) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLPutNext(self->lp,(int)token));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *mathlink_GetNext(mathlink_Link *self, PyObject *args)
{
	long token;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,token = MLGetNext(self->lp));
	CHECKEQUAL(token,MLTKERROR,self);
	
	return PyInt_FromLong(token);
}


static PyObject * mathlink_GetNextRaw(mathlink_Link *self, PyObject *args)
{
	long token;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,token = MLGetNextRaw(self->lp));
	CHECKEQUAL(token,MLTKERROR,self);
	
	return PyInt_FromLong(token);
}


static PyObject *mathlink_PutArgCount(mathlink_Link *self, PyObject *args)
{
	long i, result;
	
	if(!PyArg_ParseTuple(args, "i", &i)) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLPutArgCount(self->lp, (int)i));
	CHECKNOTEQUAL(result, MLSUCCESS, self);

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *mathlink_GetArgCount(mathlink_Link *self, PyObject *args)
{
	long result;
	int i;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLGetArgCount(self->lp, &i));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	return PyInt_FromLong(i);
}


static PyObject *mathlink_GetRawArgCount(mathlink_Link *self, PyObject *args)
{
	long result;
	int i;
	
	if(PyTuple_Size(args) > 0) return NULL;

	CheckForThreadsAndRunLink(self,result = MLGetRawArgCount(self->lp, &i));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	return PyInt_FromLong(i);
}

MLDEFN(int, mathlink_YieldFunctionHandler,(MLINK lp, MLYieldParameters yp))
{
	mathlink_Link *self;
	mathlink_Env *env;
	MLEnvironment ep;
	long count = 0;
	unsigned long sleep = 0;
	PyObject *Return;
	
	self = (mathlink_Link *)MLUserData(lp, (MLUserFunctionType *) 0);
	if(self == NULL){
	  PyErr_SetString(mathlinkError, "Unable to invoke Link object Yield function.  Internal data unavailable.");
	  return -1;  /* Not sure what to return yet here... */
	}
	
	/* Now I need to dissect Yield parameters from yp... */
	count = MLCountYP(yp);
	sleep = MLSleepYP(yp);
	
	/* Now I need to call self->yieldfunction with the paramters from yp...
	   
	   I assume that the routines SetYieldFunction and SetDefaultYieldFunction have checked
	   to make sure the Python Objects are callable.
	   
	   I also assume that I cannot get into this state(ie this function) with a NULL self->yieldfunction
	   pointer.
	*/
	
	ep = (MLEnvironment)MLEnclosingEnvironment(MLinkEnvironment(lp));

	env = (mathlink_Env *)MLEnvironmentData(ep);
	if(env == (mathlink_Env *)0) return 0;
	
	if((env->DefaultYieldFunction == (PyObject *)0) && (self->yieldfunction.func == (PyObject *)0)) return 0;
	else if((env->DefaultYieldFunction == (PyObject *)0) && (self->yieldfunction.func != (PyObject *)0)) Return = PyObject_CallFunction(self->yieldfunction.func, "ll", count, (long)sleep);
	else if((env->DefaultYieldFunction != (PyObject *)0) && (self->yieldfunction.func == (PyObject *)0)) Return = PyObject_CallFunction(env->DefaultYieldFunction, "ll", count, (long)sleep);
	else Return = PyObject_CallFunction(self->yieldfunction.func, "ll", count, (long)sleep);

	if(Return == NULL) return 0;   /* Assume that PyObject_CallFunction set the error itself... */
	
	/* For now all Yield functions at the Python language level should return None..., in this case, we will
	   ignore anything returned... */
	   
	Py_DECREF(Return);  
	
	return 0;
}


static PyObject *mathlink_SetYieldFunction(mathlink_Link *self, PyObject *args)
{
	PyObject *YF;
	MLYieldFunctionObject yf;
	long result;
	
	/* Make sure we actually have a MLINK allocated, otherwise this is pointless ... */
	if(self->lp == (MLINK)0){
	  PyErr_SetString(mathlinkError, "Link object not completely allocated, please run one of the open methods");
	  return NULL;
	}
	 
	YF = PyTuple_GetItem(args, 0);
	if(YF == NULL) return NULL;
	
	if(! PyCallable_Check(YF)){
	  PyErr_SetString(PyExc_TypeError, "Argument not a Function or Object Method");
	  return NULL;
	}
	 
	/* Set the object in self... */
	if(self->yieldfunction.func != NULL){ Py_DECREF(self->yieldfunction.func);}
	self->yieldfunction.func = YF;
	Py_INCREF(YF);
	 
	/* Now install mathlink_YieldFunctionHandler in the link... */
	yf = MLCreateYieldFunction(self->PyEnv->env, (MLYieldFunctionType)&mathlink_YieldFunctionHandler, (MLPointer)0);
	result = MLSetYieldFunction(self->lp, yf);
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	   
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *mathlink_YieldFunction(mathlink_Link *self, PyObject *args)
{
	PyObject *YF;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	if(self->yieldfunction.func != NULL) YF = self->yieldfunction.func;
	else YF = Py_None;
	Py_INCREF(YF);

	return YF;
}

static int mathlink_DoMessage(void *so)
{
	mathlink_Link *self = (mathlink_Link *) so;
	PyObject *Return;
	PyObject *Args;
	
	Args = Py_BuildValue("ll", self->messagehandler.arg[0], self->messagehandler.arg[1]);
	if(Args == NULL) return -1; /* Assume that Py_BuildValue set the error condition itself... */

	Return = PyEval_CallObject(self->messagehandler.func, Args);
	if(Return == NULL) return -1;   /* Assume that PyObject_CallFunction set the error itself... */

	 
	Py_DECREF(Args);  
	Py_DECREF(Return);  
	
	return 0;
}

MLDEFN(void, mathlink_MessageFunctionHandler,(MLINK lp, int msg, int mark))
{
	mathlink_Link *self;

	self = (mathlink_Link *)MLUserData(lp, (MLUserFunctionType *) 0);
	if(self == NULL){
		PyErr_SetString(mathlinkError, "Unable to invoke link object message handler function.  Internal data unavailable.");
		return;  /* Not sure what to return yet here... */
	}
	
	self->messagehandler.arg[0] = msg;
	self->messagehandler.arg[1] = mark;
	
	Py_AddPendingCall(mathlink_DoMessage, (void *)self);
	
	return;
}


static PyObject *mathlink_SetMessageHandler(mathlink_Link *self, PyObject *args)
{
	mathlink_Env *env;
	PyObject *YF;
	long result;
	MLMessageHandlerObject mho;
	
	/* Make sure we actually have a MLINK allocated, otherwise this is pointless ... */
	if(self->lp == (MLINK)0){
		PyErr_SetString(mathlinkError, "Link object not completely allocated, please run one of the open methods");
		return NULL;
	}
	 
	YF = PyTuple_GetItem(args, 0);
	if(YF == NULL) return NULL;
	
	if(! PyCallable_Check(YF)){
	  PyErr_SetString(PyExc_TypeError, "Argument not a Function or Object Method");
	  return NULL;
	}
	
	/* Now we set the function in self... */
	if(self->messagehandler.func != NULL){ Py_DECREF(self->messagehandler.func); }
	self->messagehandler.func = YF;
	Py_INCREF(YF);
	
	env = self->PyEnv;

	/* Now we install the binding message handler function... */
	mho = MLCreateMessageHandler(env->env, (MLMessageHandlerType)mathlink_MessageFunctionHandler, (MLPointer)0);
	result = MLSetMessageHandler(self->lp, mho);
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *mathlink_MessageHandler(mathlink_Link *self, PyObject *args)
{
	PyObject *YF;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	YF = self->messagehandler.func;
	if(YF == NULL) YF = Py_None;
	
	Py_INCREF(YF);
	
	return YF;
}


static PyObject *mathlink_GetMessage(mathlink_Link *self, PyObject *args)
{ 
	long result;
	int msg, arg;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLGetMessage(self->lp, &msg, &arg));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	return Py_BuildValue("(ll)",msg,arg);
}


static PyObject *mathlink_MessageReady(mathlink_Link *self, PyObject *args)
{
	long result;
	
	if(PyTuple_Size(args) > 0) return NULL;

	CheckForThreadsAndRunLink(self,result = MLMessageReady(self->lp));

	return Py_BuildValue("l",result);
}


static PyObject *mathlink_PutMessage(mathlink_Link *self,PyObject *args)
{
	long result,message;
	
	if(! PyArg_ParseTuple(args,"l",&message)) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLPutMessage(self->lp,(int)message));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}

	 
static PyObject * mathlink_CreateMark(mathlink_Link *self, PyObject *args)
{
	MLINKMark mp;
	mathlink_Mark *mo;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	if(self->lp == (MLINK)0){
		PyErr_SetString(mathlinkError,"Link not yet completed.  Please call one of the open methods");
		return NULL;
	}
	
	CheckForThreadsAndRunLink(self,mp = MLCreateMark(self->lp));
	if(mp == (MLINKMark)0){
	  mathlink_SetErrorConditionFromLink(self);
	  return NULL;
	}
	
	mo = (mathlink_Mark *) PyObject_New(mathlink_Mark, &mathlink_MarkType);
	if(mo == NULL) return NULL;

	mo->mp = mp;
	mo->link = (PyObject *)self;
	Py_INCREF(self);
	
	return (PyObject *)mo;
}


static PyObject * mathlink_SeekToMark(mathlink_Link *self, PyObject *args)
{
	mathlink_Mark *mo = NULL;
	long index;
	MLINKMark newmark;
	
	if(!PyArg_ParseTuple(args,"Ol",(PyObject **)&mo, &index)) return NULL;
	
	if(PyObject_Type((PyObject *)mo) != (PyObject *)&mathlink_MarkType){
		PyErr_SetString(PyExc_TypeError,"Argument is not a mark object");
		return NULL;
	}
	
	if(self != (mathlink_Link *)mo->link){
		PyErr_SetString(mathlinkError,"Mark not set for this link");
		return NULL;
	}
	
	CheckForThreadsAndRunLink(self,newmark = MLSeekToMark(self->lp, mo->mp, (int)index));
	if(newmark == (MLINKMark)0){
		mathlink_SetErrorConditionFromLink(self);
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_SeekMark(mathlink_Link *self, PyObject *args)
{
	mathlink_Mark *mo = NULL;
	long index;
	MLINKMark newmark;
	
	if(!PyArg_ParseTuple(args,"Ol",(PyObject **)&mo, &index)) return NULL;
	
	if(PyObject_Type((PyObject *)mo) != (PyObject *)&mathlink_MarkType){
		PyErr_SetString(PyExc_TypeError,"Argument is not a mark object");
		return NULL;
	}
	
	if(self != (mathlink_Link *)mo->link){
		PyErr_SetString(mathlinkError,"Mark not set for this link");
		return NULL;
	}
	
	CheckForThreadsAndRunLink(self,newmark = MLSeekMark(self->lp, mo->mp, (int)index));
	if(newmark == (MLINKMark)0){
		mathlink_SetErrorConditionFromLink(self);
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_TransferExpression(mathlink_Link *self, PyObject *args)
{
	mathlink_Link *Arg = NULL;
	long result;
	
	if(! PyArg_ParseTuple(args,"O",(PyObject **)&Arg)) return NULL;
	
	if(PyObject_Type((PyObject *)Arg) != (PyObject *)&mathlink_LinkType){
		PyErr_SetString(PyExc_TypeError, "Argument is not a link object");
		return NULL;
	}
	
	if(! self->connected){
		PyErr_SetString(mathlinkError, "Link not connected");
		return NULL;
	}
	if(! Arg->connected){
		PyErr_SetString(mathlinkError, "Argument link not connected");
		return NULL;
	}
	
	CheckForThreadsAndRunLink(self,result = MLTransferExpression(self->lp,Arg->lp));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_Ready(mathlink_Link *self, PyObject *args)
{
	long result;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLReady(self->lp));

	return Py_BuildValue("i",result);
}

MLINK mathlink_NewLink(mathlink_Env *self, PyObject *args, PyObject *keywords)
{
	char *name = "";
	char *protocol = "";
	int mode = -1;
	char *host = "";
	int options = 256;
	int linklaunch = 0;
	int linkcreate = 0;
	int linkconnect = 0;
	char *authentication = "";
	char *device = "";
	int namelen, protlen, hostlen, authlen, devlen, buflen = 0;
	int makelink = 0;
	char *buf, *tmpbuf;

	MLINK lp;
	int err;

	if(!PyArg_ParseTupleAndKeywords(args,keywords,"|ssisiiiiss",commandlinelist,&name,&protocol,&mode,&host,&options,&linklaunch,&linkcreate,&linkconnect,&authentication,&device)){
		return NULL; /* Exception ? */
	}

	/* Now we have to construct the string to use in MLOpenString... */
	
	namelen = (int)strlen(name);
	protlen = (int)strlen(protocol);
	hostlen = (int)strlen(host);
	authlen = (int)strlen(authentication);
	devlen  = (int)strlen(device);
	
	if(namelen > 0){ buflen += 11 + namelen;  makelink = 1; }         /* -linkname (10 characters), name + space */
	if(protlen > 0){ buflen += 15 + protlen;  makelink = 1; }         /* -linkprotocol (14 characters, name + space */
	if(mode != -1){ buflen += 13; makelink = 1; }                     /* -linkmode (10 characters), name + space */
	if(hostlen > 0){ buflen += 11 + hostlen; makelink = 1; }          /* -linkhost (10 characters), name + space */
	if(options != 256){ buflen += 17; makelink = 1; }                 /* -linkoptions (13 characters), name + space */
	if(linklaunch != 0){ buflen += 12; makelink = 1; }
	if(linkcreate != 0){ buflen += 12; makelink = 1; }
	if(linkconnect != 0){ buflen += 13; makelink = 1; }
	if(authlen > 0){ buflen += 21 + authlen; makelink = 1; }          /* -linkauthentication (20 characters), name + space */
	if(devlen > 0){ buflen += 12 + devlen; makelink = 1; }            /* -linkdevice (12 characters), name + space */

	/* Allocate the buffer space */
	buf = PyMem_New(char, buflen + 1);
	memset((void *)buf, 0, buflen + 1);
	if(buf == NULL){
		PyErr_SetString(PyExc_MemoryError, "Unable to allocate internal memory for link creation");
		return NULL;
	}
	tmpbuf = buf;

	if(namelen > 0){
		sprintf(tmpbuf, "-linkname %s ", name);
		tmpbuf += strlen(tmpbuf);   /* 11 + namelen */
	}
	if(protlen > 0){
		sprintf(tmpbuf, "-linkprotocol %s ", protocol);
		tmpbuf += strlen(tmpbuf);   /* 15 + protlen */
	}
	if(mode != -1){
		sprintf(tmpbuf, "-linkmode %d ", mode);
		tmpbuf += strlen(tmpbuf);   /* 13 at most */
	}
	if(hostlen > 0){
		sprintf(tmpbuf, "-linkhost %s ", host);
		tmpbuf += strlen(tmpbuf);   /* 10 + hostlen + 1 */
	}
	if(options != 256){
		sprintf(tmpbuf, "-linkoptions %d ", options);
		tmpbuf += strlen(tmpbuf);   /* at most 17 */
	}
	if(linklaunch != 0){
		sprintf(tmpbuf, "-linklaunch ");
		tmpbuf += 12;
	}
	if(linkcreate != 0){
		sprintf(tmpbuf, "-linkcreate ");
		tmpbuf += 12;
	}
	if(linkconnect != 0){
		sprintf(tmpbuf, "-linkconnect ");
		tmpbuf += 13;
	}
	if(authlen > 0){
		sprintf(tmpbuf, "-linkauthentication %s ", authentication);
		tmpbuf += strlen(tmpbuf);    /* 20 + authlen + 1 */
	}
	if(devlen > 0){
		sprintf(tmpbuf, "-linkdevice %s ", device);
		tmpbuf += strlen(tmpbuf);    /* 12 + devlen + 1 */
	}

	/* Now the buffer should have the string we want... */
	CheckForThreadsAndRunEnv(self,lp = MLOpenString(self->env, buf, &err));

	if(err != MLEOK){
		mathlink_SetErrorConditionFromCode(err);
		PyMem_Free((void *)buf); /* free the buf memory first... */
		return NULL;
	}
	
	/* Now we free buf because we are finished with it. */
	PyMem_Free((void *)buf);

	return lp;
}


static PyObject * mathlink_GetName(mathlink_Link *self, PyObject *args)
{
	const char *string;
	PyObject *NString;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	string = (const char *)MLName(self->lp);
	
	if(string != NULL){
		/* Create a new String Object */
		NString = PyString_FromString(string);
		if(NString == NULL) return NULL;
		return NString;
	}
	else{
		Py_INCREF(Py_None);
		return Py_None;
	}
}


static PyObject * mathlink_SetName(mathlink_Link *self, PyObject *args)
{
	char *newmlstring; 
	const char *string;
	 
	if(! PyArg_ParseTuple(args,"s",&string)) return NULL;
	 
	newmlstring = MLSetName(self->lp,string);
	if(newmlstring == NULL){
		mathlink_SetErrorConditionFromLink(self);
		return NULL;
	}
	 
	/* Return Py_None */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_Flush(mathlink_Link *self, PyObject *args)
{
	int result;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLFlush(self->lp));
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_Duplicate(mathlink_Link *self, PyObject *args)
{
	mathlink_Link *New;
	char *name;
	int err;
	
	if(! PyArg_ParseTuple(args,"s",&name)) return NULL;
	
	/* Create the new Python link object */
	New = PyObject_New(mathlink_Link, &mathlink_LinkType);
	if(New == NULL) return NULL;
	
	New->lp = MLDuplicateLink(self->lp, (const char *) name, &err);
	CHECKNOTEQUAL(err,MLEOK,self);
	
	if(New->lp == NULL){
		PyObject_Del(New);
		PyErr_SetString(mathlinkError,"Error duplicating link");
		return NULL;
	}
	
	/* Copy the old yieldfunction and messagehandler... */
	if(self->yieldfunction.func != NULL){
		New->yieldfunction.func = self->yieldfunction.func;
		Py_INCREF(self->yieldfunction.func);
	}
	if(self->messagehandler.func != NULL){
		New->messagehandler.func = self->messagehandler.func;
		Py_INCREF(self->messagehandler.func);
	}
	
	return (PyObject *)New;
}


static mathlink_Link * mathlink_Open(mathlink_Env *self, PyObject *args, PyObject *keywords)
{
	mathlink_Link *PyLink;

	PyLink = Link_Alloc(self);
	if(PyLink == (mathlink_Link *)0){
		PyErr_SetString(PyExc_MemoryError, "Memory allocation failed during link creation");
		return NULL;
	}

	/* Call mathlink_NewLink to open the new link object */
	PyLink->lp = mathlink_NewLink(self,args,keywords);
	if(PyLink->lp == NULL){
		PyErr_SetString(mathlinkError, "Unable to create new link object");
		return NULL;
	}
	
	/* We must install address of the mathlink_Link self... */
	MLSetUserData(PyLink->lp, (MLPointer)PyLink, (MLUserFunctionType) 0);
	
	/* Now we check to see if we need to install the DefaultYieldFunction... */
	if((self->DefaultYieldFunction != NULL) && (PyLink->yieldfunction.func != self->DefaultYieldFunction)){
		PyLink->yieldfunction.func = self->DefaultYieldFunction;
		Py_INCREF(self->DefaultYieldFunction);
	}
	
	return PyLink;
}


static mathlink_Link * mathlink_OpenString(mathlink_Env *self, PyObject *args)
{
	int err;
	mathlink_Link *PyLink;
	const char *string;

	if(! PyArg_ParseTuple(args,"s",&string)) return NULL;

	/* Create the Python Link object */
	PyLink = Link_Alloc(self);
	if(PyLink == (mathlink_Link *)0){
		PyErr_SetString(PyExc_MemoryError, "Memory allocation failed during link creation");
		return NULL;
	}

	/* Create the MLINK object. */
	CheckForThreadsAndRunEnv(self,PyLink->lp = MLOpenString(self->env, string, &err));

	if(err != MLEOK){
		mathlink_SetErrorConditionFromCode(err);
		PyMem_DEL(PyLink);
		return NULL;
	}

	/* We must install address of the mathlink_Link self... */
	MLSetUserData(PyLink->lp, (MLPointer)PyLink, (MLUserFunctionType) 0);
	
	/* Do we need to install a default yield function... */
	if((self->DefaultYieldFunction != NULL) && (PyLink->yieldfunction.func != self->DefaultYieldFunction)){
		PyLink->yieldfunction.func = self->DefaultYieldFunction;
		Py_INCREF(self->DefaultYieldFunction);
	}
	
	return PyLink;
}


static mathlink_Link * mathlink_OpenArgv(mathlink_Env *self, PyObject *args)
{
	int err;
	char *buf, *tmp;
	char **newarray;
	int argvsize = 0,newbufsize = 0;
	PyObject *Argv;
	mathlink_Link *PyLink;
	
	/* Get the first argument because that should correspond to a list of argv, usually sys.argv */
	Argv = PyTuple_GetItem(args,0);
	if(Argv == NULL){
		PyErr_SetString(PyExc_TypeError, "Unable to retrieve arg[0] from function argument tuple");
		return NULL;
	}
	
	
	/* Check to make sure the argument is a list */
	if(!PyList_Check(Argv)){
		PyErr_SetString(PyExc_TypeError, "The first argument is not a List object");
		return NULL;
	}
	
	/* Get the list size */
	argvsize = (int)PyList_Size(Argv);

	/* Now we create the C string array */
	if(argvsize != 0){
		int i;
		char *string;
		PyObject *String;
	  
		/* Allocate Memory... */
		newarray = PyMem_New(char *, argvsize + 1);
		memset((void *) newarray, 0, argvsize + 1);
		if(newarray == (char **)0){
			PyErr_SetString(PyExc_MemoryError, "Memory allocation failed during link creation");
			return NULL;
		}
		/* Now lets calculate the newbufsize.  At the same time we will check to make
		sure that each item in the List is actually a String object.  If not we will
		generate a TypeError exception. */
	  
		for(i = 0; i < argvsize; i++){
			String = PyList_GetItem(Argv,i);
			if(String == NULL){  /* Check for validity of the object */
				char errmsg[ERRMSGLEN];
				sprintf(errmsg, "Invalid object encountered at List index %d", i);
				PyErr_SetString(PyExc_TypeError, errmsg);
				PyMem_Free((void *)newarray);
				return NULL;
			}
			if(!PyString_Check(String)){  /* Check for validity of the object as a String */
				char errmsg[ERRMSGLEN];
				sprintf(errmsg, "Object encountered at List index %d not a String object", i);
				PyErr_SetString(PyExc_TypeError, errmsg);
				PyMem_Free((void *)newarray);
				return NULL;
			}
	    
			newbufsize += (int)PyString_Size(String) + 1;
		}
	  
		/* Now allocate space for the new string array... */
		buf = PyMem_New(char,newbufsize);
		memset((void *)buf, 0, newbufsize);
		if(buf == NULL){
			PyErr_SetString(PyExc_MemoryError,"Failed to allocate memory necessary for creating link object");
			PyMem_Free((void *)buf);
			PyMem_Free((void *)newarray);
			return NULL;
		}
		tmp = buf;
	  
		/* Now populate the new space with the strings from the List... */
		for(i = 0; i < argvsize; i++){
			String = PyList_GetItem(Argv,i);
			if(String == NULL){  /* Verify the String's validity */
				char errmsg[ERRMSGLEN];
				sprintf(errmsg, "Error accessing String at index %d", i);
				PyErr_SetString(mathlinkError, errmsg);
				PyMem_Free((void *)buf);
				PyMem_Free((void *)newarray);
				return NULL;
			}
			newarray[i] = tmp;
			/* Get the C version of the string... */
			string = PyString_AsString(String);
			if(string == NULL){
				PyErr_SetString(mathlinkError, "Error converting String object to C string");
				PyMem_Free((void *)buf);
				PyMem_Free((void *)newarray);
				return NULL;
			}
			memcpy(tmp,string,strlen(string));
			tmp += strlen(string) + 1;
		}
	  
	/* At this point buf is full... */
	}
	else{
		buf = (char *)0;
		newarray = (char **)0;
	}

	PyLink = Link_Alloc(self);
	if(PyLink == (mathlink_Link *)0){
		PyErr_SetString(PyExc_MemoryError, "Memory allocation failed during link creation");
		if(argvsize > 0){
			PyMem_Free((void *)buf);
			PyMem_Free((void *)newarray);
		}
		return NULL;
	}

	/* Now call MLOpenArgv... */
	
	CheckForThreadsAndRunEnv(self,PyLink->lp = MLOpenArgv(self->env, newarray, newarray + argvsize, &err));

	if(PyLink->lp == (MLINK)0){
		PyErr_SetString(mathlinkError,"MLOpenArgv error generating new link object");
		if(argvsize > 0){
			PyMem_Free((void *)buf);
			PyMem_Free((void *)newarray);
		}
		return NULL;
	}
	if(err != MLEOK){
		mathlink_SetErrorConditionFromLink(PyLink);
		if(argvsize > 0){
			PyMem_Free((void *)buf);
			PyMem_Free((void *)newarray);
		}
		return NULL;
	}
	
	if(argvsize > 0){
		int i;
	  
		/* zero out newarray... */
		for(i = 0; i < argvsize; i++) newarray[i] = (char *)0;
	  
		/* Now free memory... */
		PyMem_Free((void *)buf);
		PyMem_Free((void *)newarray);
	}
	
	/* We must install address of the mathlink_Link self... */
	MLSetUserData(PyLink->lp, (MLPointer)PyLink, (MLUserFunctionType) 0);
	
	/* Do we need to install a default yield function... */
	if((self->DefaultYieldFunction != NULL) && (PyLink->yieldfunction.func != self->DefaultYieldFunction)){
		PyLink->yieldfunction.func = self->DefaultYieldFunction;
		Py_INCREF(self->DefaultYieldFunction);
	}
	
	return PyLink;
}
	  

static mathlink_Link * mathlink_LoopbackOpen(mathlink_Env *self, PyObject *args)
{
	int err;
	mathlink_Link *PyLink;
	
	if(PyTuple_Size(args) > 0) return NULL;

	PyLink = Link_Alloc(self);
	if(PyLink == (mathlink_Link *)0){
		PyErr_SetString(PyExc_MemoryError, "Memory allocation failed during link creation");
		return NULL;
	}

	CheckForThreadsAndRunEnv(self,PyLink->lp = MLLoopbackOpen(self->env,&err));

	if(PyLink->lp == NULL){
		PyErr_SetString(mathlinkError, "Failed to create Loopback link object");
		return  NULL;
	}
	CHECKNOTEQUAL(err,MLEOK,PyLink);

	/* We must install address of the mathlink_Link self... */ /* Not sure if this is necessary for a loopback link...*/
	MLSetUserData(PyLink->lp, (MLPointer)PyLink, (MLUserFunctionType) 0);
	
	if((self->DefaultYieldFunction != NULL) && (PyLink->yieldfunction.func != self->DefaultYieldFunction)){
		PyLink->yieldfunction.func = self->DefaultYieldFunction;
		Py_INCREF(self->DefaultYieldFunction);
	}
	
	return PyLink;
}


static PyObject * mathlink_Connect(mathlink_Link *self, PyObject *args)
{
	long result;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	CheckForThreadsAndRunLink(self,result = MLConnect(self->lp));
	if(result != MLSUCCESS){
		PyErr_SetString(mathlinkError, "Failure to connect the link");
		return NULL;
	}
	
	self->connected = 1;
	
	/* Return Py_None */
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_EstablishString(mathlink_Link *self, PyObject *args, PyObject *keywords)
{
	int mathlink = 3, decoders = -1, newlen = 0;
	char *numericsid = NULL, *tokens = NULL, *textid = NULL, *formats = NULL, *greeting, *tmp;
	char *argumentlist[] = {
		"mathlink", "decoders", "numericsid", "tokens", "textid", "formats", NULL};
	long result;
	
	if(! PyArg_ParseTupleAndKeywords(args,keywords,"|issssi",argumentlist,&mathlink, &decoders, &numericsid, &tokens, &textid, &formats, &decoders)){
	  return NULL;
	}
	
	/* Now construct the greeting string... */
	newlen += 11;  /* "MathLink # " */
	if(numericsid != NULL) newlen += (int)strlen(numericsid) + 13;   /* includes '-numericsid ' */
	if(tokens != NULL) newlen += (int)strlen(tokens) + 9;            /* includes '-tokens ' */
	if(textid != NULL) newlen += (int)strlen(textid) + 9;            /* includes '-textid ' */
	if(formats != NULL) newlen += (int)strlen(formats) + 10;          /* includes '-formats ' */
	if(decoders != -1) newlen += 10 + 11;                       /* includes '-decoders '; At most a 10 digit decoder number... */

	greeting = (char *)PyMem_New(char, newlen + 1);
	if(greeting == (char *)0){
		PyErr_NoMemory();
		return NULL;
	}
	memset((void *)greeting, 0, newlen + 1);
	
	tmp = greeting;
	memcpy(tmp,"MathLink ",9);
	tmp += 9;
	sprintf(tmp, "%d ",mathlink);
	tmp += 2;
	if(numericsid != NULL){
		sprintf(tmp, "-numericsid %s", numericsid); 
		tmp += strlen(tmp);
	}
	if(tokens != NULL){
		sprintf(tmp, "-tokens %s", tokens);
		tmp += strlen(tmp);
	}
	if(textid != NULL){
		sprintf(tmp, "-textid %s", textid);
		tmp += strlen(tmp);
	}
	if(formats != NULL){
		sprintf(tmp, "-formats %s", formats);
		tmp += strlen(tmp);
	}
	if(decoders != -1){
		sprintf(tmp, "-decoders %d", decoders);
		tmp += strlen(tmp);
	}
	
	/* Now call MLEstablishString... */
	
	CheckForThreadsAndRunLink(self,result = MLEstablishString(self->lp,(const char *)greeting));

	PyMem_Free(greeting);  
	CHECKNOTEQUAL(result,MLSUCCESS,self);
	  
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_DeviceInformation(mathlink_Link *self, PyObject *args)
{
	long selector;
	PyObject *Result;
	long result;
	
	if(! PyArg_ParseTuple(args,"l", &selector)) return NULL;
	
	if(self->lp == (MLINK)0){
		PyErr_SetString(mathlinkError, "Link structure incomplete.  Please use one of the open* functions");
		return NULL;
	}
	
	switch(selector){
		long type,bufflen;
		unsigned long ul;
		unsigned short us;
		char name[MLDEVICE_BUFF_LEN];
		int fd;
	
		case MLDEVICE_TYPE:
			bufflen = sizeof(type);  
			CheckForThreadsAndRunLink(self,result = MLDeviceInformation(self->lp, MLDEVICE_TYPE, (void *)&type, &bufflen));
			CHECKNOTEQUAL(result,MLSUCCESS,self);
			Result = PyInt_FromLong(type);
			break;
		case MLDEVICE_NAME:
			bufflen = MLDEVICE_BUFF_LEN;
			CheckForThreadsAndRunLink(self,result = MLDeviceInformation(self->lp,MLDEVICE_NAME, (void *)&name, &bufflen));
			CHECKNOTEQUAL(result,MLSUCCESS,self);
			Result = PyString_FromString(name);
			break;
		case MLDEVICE_WORLD_ID:
			bufflen = MLDEVICE_BUFF_LEN;
			CheckForThreadsAndRunLink(self,result = MLDeviceInformation(self->lp,MLDEVICE_WORLD_ID, (void *)&name, &bufflen));
			CHECKNOTEQUAL(result,MLSUCCESS,self);
			Result = PyString_FromString(name);
			break;
		case PIPE_FD:
			bufflen = sizeof(fd);
			CheckForThreadsAndRunLink(self,result = MLDeviceInformation(self->lp,PIPE_FD, (void *)&fd, &bufflen));
			CHECKNOTEQUAL(result,MLSUCCESS,self);
			Result = PyInt_FromLong((long)fd);
			break;
		case PIPE_CHILD_PID:
			bufflen = sizeof(fd);
			CheckForThreadsAndRunLink(self,result = MLDeviceInformation(self->lp,PIPE_CHILD_PID, (void *)&fd, &bufflen));
			CHECKNOTEQUAL(result,MLSUCCESS,self);
			Result = PyInt_FromLong((long)fd);
			break;      
		case SOCKET_FD:
			bufflen = sizeof(fd);
			CheckForThreadsAndRunLink(self,result = MLDeviceInformation(self->lp,SOCKET_FD, (void *)&fd, &bufflen));
			CHECKNOTEQUAL(result,MLSUCCESS,self);
			Result = PyInt_FromLong((long)fd);
			break;
		case SOCKET_PARTNER_ADDR:
			bufflen = sizeof(ul);
			CheckForThreadsAndRunLink(self,result = MLDeviceInformation(self->lp,SOCKET_PARTNER_ADDR, (void *)&ul, &bufflen));
			CHECKNOTEQUAL(result,MLSUCCESS,self);
			Result = PyInt_FromLong(ul);
		case SOCKET_PARTNER_PORT:
			bufflen = sizeof(us);
			CheckForThreadsAndRunLink(self,result = MLDeviceInformation(self->lp,SOCKET_PARTNER_PORT,(void *)&us, &bufflen));
			CHECKNOTEQUAL(result,MLSUCCESS,self);
			Result = PyInt_FromLong((long)us);
			break;
		default:
			Result = Py_None;
			Py_INCREF(Result);
			break;
	}
	 
	return Result;
}


static PyObject * mathlink_FeatureString(mathlink_Link *self, PyObject *args)
{
	PyObject *String;
	char *buff;
	long result;
	
	if(PyTuple_Size(args) > 0) return NULL;
	
	buff = (char *)PyMem_New(char,FEATURESTRINGSIZE);
	memset((void *)buff,0,FEATURESTRINGSIZE);
	
	if(buff == (char *)0){
		PyErr_NoMemory();
		return NULL;
	}
	
	CheckForThreadsAndRunLink(self,result = MLFeatureString(self->lp, buff, FEATURESTRINGSIZE));
	
	if(result != 0){
		PyErr_SetString(mathlinkError, "Buffer overflow occurred");
		PyMem_Free(buff);
		return NULL;
	}
	
	String = PyString_FromString(buff);
	PyMem_Free(buff);
	
	return String;
}

	   
static PyObject * mathlink_Close(mathlink_Link *self, PyObject *args)
{  
	/* Check for 0 args... */
	if(PyTuple_Size(args) > 0) return NULL;              /* exception */
	
	
	/* Close the link... */
	CheckForThreadsAndRunLink(self,MLClose(self->lp));

	self->lp = (MLINK)0;
	
	/* Return None... */
	
	Py_INCREF(Py_None);
	return Py_None;
}




/* Create a New Link */
static mathlink_Link * Link_Alloc(mathlink_Env *self)
{
	mathlink_Link *newself;
	
	/* Now we need to create the new mathlink_Link */
	newself = PyObject_New(mathlink_Link, &mathlink_LinkType);
	if(newself == NULL) return NULL;

	newself->autoclear = 0;
	newself->connected = 0;
	if(self->DefaultYieldFunction != NULL){
		newself->yieldfunction.func = self->DefaultYieldFunction;
		Py_INCREF(self->DefaultYieldFunction);
	}
	else newself->yieldfunction.func = NULL;
	newself->messagehandler.func = NULL;

	newself->lp = (MLINK)0;
	newself->PyEnv = self;

	Py_INCREF(self);

	return newself;
}

static void Link_Dealloc(mathlink_Link *self)
{
	if(self->lp != (MLINK)0){ MLClose(self->lp);}

	/* This DECREF must come after we destroy the MLINK object. */
	Py_DECREF(self->PyEnv);
	
	Py_XDECREF(self->yieldfunction.func);
	Py_XDECREF(self->messagehandler.func);
	PyObject_Del(self);
}

static PyObject * Link_Getattr(mathlink_Link *self, char *name)
{
	/* Look for an instance method */
	return Py_FindMethod(mathlink_Link_methods, (PyObject *)self, name);
}

static PyObject * Link_Repr(mathlink_Link *self)
{
	char *mlbuffer;
	const char *linkname;
	PyObject *NString;
	
	/* Get the link name... */
	linkname = (const char *)MLName(self->lp);
	if(linkname == NULL) linkname = "No Name";
	
	/* Now create memory space for the string... */
	mlbuffer = PyMem_New(char, 8 + strlen(linkname));
	memset((void *)mlbuffer, 0, 8 + strlen(linkname));
	
	sprintf(mlbuffer, "<Link %s>", linkname);
	
	NString = PyString_FromString(mlbuffer);
	PyMem_Free((void *)mlbuffer);
	
	return NString;
}


/* It is important to recognize that this function, like the mathlink API call MLSetDefaultYieldFunction
	 will not update any yield function information for links that already exist.  The mathlink
	 API call sets the default yield function in the environment pointer and all subsequent links will
	 use that yield function.  However already existing links will not use that function.  At this
	 point I want to emulate the behaviour of the mathlink API call. */
static PyObject * mathlink_SetDefaultYieldFunction(mathlink_Env *self, PyObject *args)
{
	PyObject *YF;
	MLYieldFunctionObject yf;
	long result;

	/* Get the YieldFunction... */
	YF = PyTuple_GetItem(args, 0);
	if(YF == NULL) return NULL;

	if(! PyCallable_Check(YF)){
		PyErr_SetString(PyExc_TypeError, "Argument not a Function or Object Method");
		return NULL;
	}

	if(self->DefaultYieldFunction != (PyObject *)0){ Py_DECREF(self->DefaultYieldFunction); }
	self->DefaultYieldFunction = YF;
	Py_INCREF(YF);
	
	/* Install the mathlink_YieldFunctionHandler in the library... */
	yf = MLCreateYieldFunction(self->env, (MLYieldFunctionType)&mathlink_YieldFunctionHandler, (MLPointer)0);
	result = MLSetDefaultYieldFunction(self->env, yf);
	if(result != MLSUCCESS){
		mathlink_SetErrorConditionFromCode(MLEUNKNOWN);
		return NULL;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_DefaultYieldFunction(mathlink_Env *self, PyObject *args)
{
	PyObject *YF;
	
	if(PyTuple_Size(args) > 0) return NULL;

	YF = self->DefaultYieldFunction;

	if(YF == (PyObject *)0) YF = Py_None;

	Py_INCREF(YF);
	
	return YF;
}



static PyObject * mathlink_GetLink(mathlink_Mark *self, PyObject *args)
{
	if(PyTuple_Size(args) > 0) return NULL;
	
	Py_INCREF(self->link);
	return self->link;
} 


/*  Basic Operations */

static mathlink_Link * mathlink_Align(PyObject *self, PyObject *args, PyObject *keywords)
{
	return NULL;
}


/* Create a new Environment Object */
static mathlink_Env * Env_Alloc(PyObject *self, PyObject *args)
{
	mathlink_Env *newenv;
	MLParameters p;
	ulong_ct result;
	long err;
	int arg = 0;
	int doparameters = 0;

	if(PyTuple_Size(args) > 0)
		if(PyArg_ParseTuple(args, "|i", &arg)) doparameters = 1;

	newenv = PyObject_New(mathlink_Env, &mathlink_EnvType);
	if(newenv == (mathlink_Env *)0) goto E0;

	result = MLNewParameters((MLParametersPointer) &p, MLREVISION, MLAPIREVISION);
	if(result == 0) goto E1;

#if USE_PYTHON_MEMORY_ALLOCATOR
	/* We install Python's allocator and deallocator in order to allow the interpreter to more closely track
	   memory usage */
	MLSetAllocParameter((MLParametersPointer) &p, (MLAllocator) mathlink_MemMallocWrapper, (MLDeallocator) mathlink_MemFreeWrapper);
#endif

	if(doparameters){
		err = MLSetEncodingParameter((MLParametersPointer)&p, (uint_ct)arg);
		if(err != MLEOK) goto E1;
	}

	Py_BEGIN_ALLOW_THREADS
	newenv->env = MLInitialize((MLParametersPointer) p);
	Py_END_ALLOW_THREADS

	if(newenv->env == (MLENV)0) goto E1;

	newenv->ConfirmFunction = (PyObject *)0;
	newenv->AlertFunction = (PyObject *)0;
	newenv->RequestFunction = (PyObject *)0;
	newenv->RequestArgvFunction = (PyObject *)0;
	newenv->RequestToInteractFunction = (PyObject *)0;

	newenv->DefaultYieldFunction = (PyObject *)0;

	/* We install the Python Env object in the MathLink Env object.  Later we may need access to
	the Python Env object from our functions called directly via MathLink. */
	MLSetEnvironmentData(newenv->env, (MLPointer)newenv);

	return newenv;

E1:	PyObject_Del(newenv);
E0:	return (mathlink_Env *)0;
}


static void Env_Dealloc(mathlink_Env *self)
{
	if(self->env != (MLENV)0) MLDeinitialize(self->env);
	
	Py_XDECREF(self->ConfirmFunction);
	Py_XDECREF(self->AlertFunction);
	Py_XDECREF(self->RequestFunction);
	Py_XDECREF(self->RequestArgvFunction);
	Py_XDECREF(self->RequestToInteractFunction);
	Py_XDECREF(self->DefaultYieldFunction);

	PyObject_Del(self);
}


static PyObject * Env_Getattr(mathlink_Env *self, char *name)
{
	/* Look for an instance method */
	return Py_FindMethod(mathlink_Env_methods, (PyObject *)self, name);
}


static PyObject * Env_Repr(mathlink_Env *self)
{
	PyObject *NString;
	
	NString = PyString_FromString("<env>");
	
	return NString;
}


static mathlink_Link * mathlink_ReadyParallel(mathlink_Env *self, PyObject *args)
{
	PyObject *list;
	PyObject *pytimeout;
	PyObject *obj;
	mathlink_Link *rval;
	MLINK *linklist;
	mathlink_Link *link;
	mltimeval timeout;
	int listsize, i;
	long error;

	if(PyTuple_Size(args) <= 0){
		PyErr_SetString(mathlinkReadyParallelError, "Not enough arguments for readyparallel");
		goto E0;
	}
	
	list = PyTuple_GetItem(args, 0);
	pytimeout = PyTuple_GetItem(args, 1);

	listsize = (int)PyList_Size(list);

	if(listsize == 0){
		PyErr_SetString(mathlinkReadyParallelError, "List of links empty");
		goto E0;
	}
	
	linklist = PyMem_New(MLINK, listsize);
	if(linklist == (MLINK *)0){
		PyErr_SetString(mathlinkReadyParallelError, "Error allocating memory for readyparallel call");
		goto E0;
	}

	/* Retrieve the links */
	for(i = 0; i < listsize; i++){
		obj = PyList_GetItem(list, i);
		if(! PyObject_TypeCheck(obj,&mathlink_LinkType)){
			PyErr_SetString(mathlinkReadyParallelError, "Object in link list not a link object");
			goto E1;
		}
		link = (mathlink_Link *)obj;
		*(linklist + i) = link->lp;
	}
	
	/* Update the timeout */
	if(! PyArg_ParseTuple(pytimeout, "kk", &timeout.tv_sec, &timeout.tv_usec)){
		PyErr_SetString(mathlinkReadyParallelError, "Timeout tuple does not contain the correct integer pair (seconds, microseconds)");
		goto E1;
	}

	CheckForThreadsAndRunEnv(self,error = MLReadyParallel(self->env, linklist, listsize, timeout));

	if(error == MLREADYPARALLELERROR){
		PyErr_SetString(mathlinkReadyParallelError, "readyparallel generated an internal error");
		goto E1;
	}
	else if(error == MLREADYPARALLELTIMEDOUT){
		rval = (mathlink_Link *)Py_None;
	}
	else{
		rval = (mathlink_Link *)PyList_GetItem(list, (int)error);
	}

	Py_INCREF(rval);

	PyMem_Free(linklist);
	
	return rval;

E1:	PyMem_Free(linklist);
E0:	return NULL;
}


mldlg_result mathlink_AlertStub(MLENV ep, kcharp_ct message)
{
	mathlink_Env *env;
	PyObject *Result;
	mldlg_result result;

	env = (mathlink_Env *)MLEnvironmentData(ep);
	if(env == (mathlink_Env *)0){
		PyErr_SetString(mathlinkError, "Internal error retrieving 'env' object");
		return DIALOGERROR;
	}

	if(env->AlertFunction == (PyObject *)0){
		PyErr_SetString(mathlinkError, "Alert python function called without installed function");
		return DIALOGERROR;
	}
	
	Result = PyObject_CallFunction(env->AlertFunction, "s", message);
	
	if(Result == NULL) return DIALOGERROR;
	
	if(! PyInt_Check(Result)){
		PyErr_SetString(mathlinkError, "Alert function must return an Integer object");
		return DIALOGERROR;
	}
	
	result = PyInt_AsLong(Result);
	Py_DECREF(Result);
	
	return result;
}

mldlg_result mathlink_ConfirmStub(MLENV ep, kcharp_ct question, mldlg_result defaultanswer)
{
	mathlink_Env *env;
	PyObject *Result = NULL;
	mldlg_result result = defaultanswer;

	env = (mathlink_Env *)MLEnvironmentData(ep);
	if(env == (mathlink_Env *)0){
		PyErr_SetString(mathlinkError, "Internal error retrieving 'env' object");
		return DIALOGERROR;
	}

	if(env->ConfirmFunction == (PyObject *)0){
		PyErr_SetString(mathlinkError, "Confirm Python function called without installed function");
		Py_DECREF(Result);
		return DIALOGERROR;
	}
	
	Result = PyObject_CallFunction(env->ConfirmFunction, "sl", question, defaultanswer);
	
	if(Result == NULL) return DIALOGERROR;
	
	if(! PyInt_Check(Result)){
		PyErr_SetString(mathlinkError, "Confirm function must return an Integer object");
		Py_DECREF(Result);
		return DIALOGERROR;
	}
	
	result = PyInt_AsLong(Result);
	Py_DECREF(Result);
	
	return result;
}

mldlg_result mathlink_RequestStub(MLENV ep, kcharp_ct prompt, charp_ct response, long size)
{
	mathlink_Env *env;
	PyObject *Result;
	char *string;
	long slen, nlen = 0;

	env = (mathlink_Env *)MLEnvironmentData(ep);
	if(env == (mathlink_Env *)0){
		PyErr_SetString(mathlinkError, "Internal error retrieving 'env' object");
		return DIALOGERROR;
	}
	
	if(env->RequestFunction == NULL){
		PyErr_SetString(mathlinkError, "Request Python function called without installed function");
		return DIALOGERROR;
	}
	
	Result = PyObject_CallFunction(env->RequestFunction, "s", prompt);
	
	if(Result == NULL) return DIALOGERROR;
	
	if(! PyString_Check(Result)){
		PyErr_SetString(mathlinkError,"Request function must return a String object");
		Py_DECREF(Result);
		return DIALOGERROR;
	}
	
	slen = (long)PyString_Size(Result);
	string = PyString_AsString(Result);
	if(string == (char *)0){
		Py_DECREF(Result);
		return DIALOGERROR;
	}
	
	if(slen > size) nlen = size;
	else nlen = slen;
	
	memcpy(response, string, nlen);
	
	Py_DECREF(Result);
	
	return 1;
}

mldlg_result mathlink_RequestArgvStub(MLENV ep, charpp_ct argv, long len, charp_ct buff, long size)
{
	mathlink_Env *env;
	PyObject *Result, *String;
	long llen, nlen = 0;
	char **tmp;
	
	env = (mathlink_Env *)MLEnvironmentData(ep);
	if(env == (mathlink_Env *)0){
		PyErr_SetString(mathlinkError, "Internal error retrieving 'env' object");
		return DIALOGERROR;
	}


	if(env->RequestArgvFunction == NULL){
		PyErr_SetString(mathlinkError, "Requestargv Python function called without installed function");
		return DIALOGERROR;
	}
	
	Result = PyObject_CallFunction(env->RequestArgvFunction,(char *)0);
	
	if(Result == NULL) return DIALOGERROR;
	
	if(! PyList_Check(Result)){
		PyErr_SetString(mathlinkError, "Requestargv function must return a list object");
		Py_DECREF(Result);
		return DIALOGERROR;
	}
	
	llen = (long)PyList_Size(Result);
	if(llen > len) nlen = len;
	else nlen = llen;
	
	for(tmp = argv; (tmp - argv) < nlen; tmp++){
		String = PyList_GetItem(Result, tmp-argv);
		if(String == NULL){
			Py_DECREF(Result);
			return DIALOGERROR;
		}
		if(! PyString_Check(String)){
			PyErr_SetString(mathlinkError,"Requestargv function returned a list with non-String object members");
			Py_DECREF(Result);
			return DIALOGERROR;
		}
	  
		*tmp = PyString_AsString(String);
	}
	
	Py_DECREF(Result);
	return 1;
}

mldlg_result mathlink_RequestToInteractStub(MLENV ep, mldlg_result wait)
{
	mathlink_Env *env;
	PyObject *Result;

	env = (mathlink_Env *)MLEnvironmentData(ep);
	if(env == (mathlink_Env *)0){
		PyErr_SetString(mathlinkError, "Internal error retrieving 'env' object");
		return DIALOGERROR;
	}
	
	if(env->RequestToInteractFunction == NULL){
		PyErr_SetString(mathlinkError, "Requesttointeract invoked for a Python routine without defined routine");
		return DIALOGERROR;
	}
	
	Result = PyObject_CallFunction(env->RequestToInteractFunction, "l", wait);
	
	if(! PyInt_Check(Result)){
		PyErr_SetString(mathlinkError, "Requesttointeract function must return an Integer object");
		return DIALOGERROR;
	}
	
	return PyInt_AsLong(Result);
}


static PyObject * mathlink_Alert(mathlink_Env *self, PyObject *args)
{
	const char *message = NULL;
	long result = 0;
	
	if(! PyArg_ParseTuple(args, "s", (char **)&message)) return NULL;

	CheckForThreadsAndRunEnv(self,result = MLAlert(self->env, message));
	if(result == DIALOGERROR){
		if(self->AlertFunction == NULL){ PyErr_SetString(mathlinkError, "Error generating alert message"); }
		return NULL;
	}

	return Py_BuildValue("l",result);
}  


static PyObject * mathlink_Confirm(mathlink_Env *self, PyObject *args)
{
	const char *question;
	long defaultanswer, result;
	
	if(! PyArg_ParseTuple(args, "sl", (char **)&question, (long *)&defaultanswer)) return NULL;

	CheckForThreadsAndRunEnv(self,result = MLConfirm(self->env, question, defaultanswer));
	if(result == DIALOGERROR){
		if(self->ConfirmFunction == NULL){ PyErr_SetString(mathlinkError, "Error generating confirm dialog"); }
		return NULL;
	}

	return Py_BuildValue("l",result);
}


static PyObject * mathlink_Request(mathlink_Env *self, PyObject *args)
{
	const char *prompt;
	char *response;
	long result;
	PyObject *Response;
	int a;

	if(! PyArg_ParseTuple(args,"s",(char **)&prompt)) return NULL;
	
	response = (char *)PyMem_New(char, requestbuffersize);
	if(response == (char *)0){
		PyErr_NoMemory();
		return NULL;
	}
	memset((void *)response, 0, requestbuffersize * sizeof(char));
	
	CheckForThreadsAndRunEnv(self,result = MLRequest(self->env, prompt, response, requestbuffersize));
	if(result == DIALOGERROR){
		if(self->RequestFunction == NULL){ PyErr_SetString(mathlinkError, "Error generating request dialog"); }
		PyMem_Free(response);
		return NULL;
	}
	 
	a = (int)strlen(response);
	Response = PyString_FromStringAndSize(response,strlen(response));
	if(Response == NULL){
		PyMem_Free(response);
		return NULL;
	}
	
	PyMem_Free(response);
	return Response;
}


static PyObject * mathlink_RequestArgv(mathlink_Env *self, PyObject *args)
{
	char **argv, **tmp;
	char *buff;
	PyObject *Argv, *String;
	mldlg_result result;

	if(PyTuple_Size(args) > 0) return NULL;

	argv = (char **)PyMem_New(char *, margvsize);
	if(argv == (char **)0){
		PyErr_NoMemory();
		return NULL;
	}
	memset((void *)argv, 0, margvsize * sizeof(char *));
	
	buff = (char *)PyMem_New(char, requestbuffersize);
	if(buff == (char *)0){
		PyErr_NoMemory();
		PyMem_Free(argv);
		return NULL;
	}
	memset((void *)buff, 0, requestbuffersize * sizeof(char));

	CheckForThreadsAndRunEnv(self,result = MLRequestArgv(self->env, argv, margvsize, buff, requestbuffersize - 1));
	if(result == DIALOGERROR){
		if(self->RequestArgvFunction == NULL){ PyErr_SetString(mathlinkError, "Error generating requestargv dialog"); }
		PyMem_Free(argv);
		PyMem_Free(buff);
		return NULL;
	}
	
	Argv = PyList_New(0);
	if(Argv == NULL){
		PyMem_Free(argv);
		PyMem_Free(buff);
		return NULL;
	}

	for(tmp = argv; *tmp != (char *)0; tmp++){
		String = PyString_FromString(*tmp);
		if(String == NULL){
			PyMem_Free(argv);
			PyMem_Free(buff);
			return NULL;
		}
	  
		if(PyList_Append(Argv,String) == -1){
			PyMem_Free(argv);
			PyMem_Free(buff);
			Py_DECREF(String);
			return NULL;
		}
	  
		Py_DECREF(String);
	}
	
	PyMem_Free(argv);
	PyMem_Free(buff);
	
	return Argv;
} 


static PyObject * mathlink_RequestToInteract(mathlink_Env *self, PyObject *args)
{
	long wait, result;
	
	if(! PyArg_ParseTuple(args, "l", (long *)&wait)) return NULL;
	
	/* We have to have a stub for this one anyway, so just call MLRequestToInteract... */
	CheckForThreadsAndRunEnv(self,result = MLRequestToInteract(self->env, wait));
	if(result == DIALOGERROR){
		if(self->RequestToInteractFunction == NULL){ PyErr_SetString(mathlinkError,"Error running requesttointeract"); } 
		return NULL;
	}
	
	return Py_BuildValue("l", result);
}


static PyObject * mathlink_SetDialogFunction(mathlink_Env *self, PyObject *args)
{
	long functiontype;
	PyObject *Function;
	long result;
	MLDialogProcPtr function;
	
	if(! PyArg_ParseTuple(args,"lO",(long *)&functiontype, (PyObject **)&Function)) return NULL;
	
	if(! PyCallable_Check(Function)){
		PyErr_SetString(PyExc_TypeError, "Argument must be a callable object");
		return NULL;
	}
	
	switch(functiontype){
		case MLAlertFunction:
			function = MLAlertCast((MLAlertProcPtr)mathlink_AlertStub);
			self->AlertFunction = Function;
			break;
		case MLConfirmFunction:
			function = MLConfirmCast((MLConfirmProcPtr)mathlink_ConfirmStub);
			self->ConfirmFunction = Function;
			break;
		case MLRequestFunction:
			function = MLRequestCast((MLRequestProcPtr)mathlink_RequestStub);
			self->RequestFunction = Function;
			break;
		case MLRequestArgvFunction:
			function = MLRequestArgvCast((MLRequestArgvProcPtr)mathlink_RequestArgvStub);
			self->RequestArgvFunction = Function;
			break;
		case MLRequestToInteractFunction:
			function = MLRequestToInteractCast((MLRequestToInteractProcPtr)mathlink_RequestToInteractStub);
			self->RequestToInteractFunction = Function;
			break;
		default:
			PyErr_SetString(mathlinkError, "Unrecognized dialog function selector");
			return NULL;
	}
	Py_INCREF(Function);
	
	CheckForThreadsAndRunEnv(self,result = MLSetDialogFunction(self->env, functiontype, function));
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_RequestBufferSize(PyObject *self, PyObject *args)
{
	if(PyTuple_Size(args) > 0) return NULL;
	return Py_BuildValue("l", requestbuffersize);
}


static PyObject * mathlink_SetRequestBufferSize(PyObject *self, PyObject *args)
{
	long size;
	
	if(! PyArg_ParseTuple(args, "l", (long *)&size)) return NULL;
	
	if(size > REQUESTBUFFERSIZE){
		requestbuffersize = size;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * mathlink_ArgvSize(PyObject *self, PyObject *args)
{
	if(PyTuple_Size(args) > 0) return NULL;
	
	return Py_BuildValue("l",margvsize);
}


static PyObject * mathlink_SetArgvSize(PyObject *self, PyObject *args)
{
	long size;
	
	if(! PyArg_ParseTuple(args, "l", (long *)&size)) return NULL;
	
	if(size > ARGVSIZE){
		margvsize = size;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

static void Mark_Dealloc(mathlink_Mark *self)
{
	mathlink_Link *link = (mathlink_Link *)self->link;
	CheckForThreadsAndRunLink(link,MLDestroyMark(link->lp, self->mp));
	Py_XDECREF(self->link);
	PyObject_Del(self);
}

static PyObject * Mark_Getattr(mathlink_Mark *self, char *name)
{
	/* Look for an Instance Method */
	return Py_FindMethod(mathlink_Mark_methods, (PyObject *)self, name);
}

static PyObject * Mark_Repr(mathlink_Mark *self)
{
	char *mbuffer;
	PyObject *NString;
	
	/* Now create memory space for the string... */
	mbuffer = PyMem_New(char, 7);
	if(mbuffer == (char *)0){
		PyErr_NoMemory();
		return NULL;
	}
	memset((void *)mbuffer, 0, 7);
	
	sprintf(mbuffer, "<Mark>");
	
	NString = PyString_FromString(mbuffer);
	PyMem_Free((void *)mbuffer);
	
	return NString;
}  


static PyTypeObject mathlink_EnvType = {
	PyObject_HEAD_INIT(NULL)
	0,                              /* ob_size */
	"Env",                          /* tp_name */
	sizeof(mathlink_Env),           /* tp_basicsize */
	0,                              /* tp_itemsize */
	
	/* Standard Methods */
	(destructor) Env_Dealloc,       /* tp_dealloc */
	(printfunc) 0,                  /* tp_print */
	(getattrfunc) Env_Getattr,      /* tp_getattr */
	(setattrfunc) 0,                /* tp_setattr */
	(cmpfunc) 0,                    /* tmp_compare */
	(reprfunc) Env_Repr,            /* tp_repr */
	
	/* Type Categories */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	(hashfunc) 0,                   /* tp_hash */
	(ternaryfunc) 0,                /* tp_call */
	(reprfunc) 0,                   /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,             /* tp_flags */
	"Env objects",                  /* tp_doc */
};


static PyTypeObject mathlink_LinkType = {
	PyObject_HEAD_INIT(NULL)
	0,                              /* ob_size */
	"Link",                         /* tp_name */
	sizeof(mathlink_Link),          /* tp_basicsize */
	0,                              /* tp_itemsize */
	
	/* Standard Methods */
	(destructor) Link_Dealloc,      /* tp_dealloc */
	(printfunc) 0,                  /* tp_print */
	(getattrfunc) Link_Getattr,     /* tp_getattr */
	(setattrfunc) 0,                /* tp_setattr */
	(cmpfunc) 0,                    /* tp_compare */
	(reprfunc) Link_Repr,           /* tp_repr */
	
	/* Type Categories */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	(hashfunc) 0,                   /* tp_hash */
	(ternaryfunc) 0,                /* tp_call */
	(reprfunc) 0,                   /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,             /* tp_flags */
	"Link objects",                 /* tp_doc */
};

static PyTypeObject mathlink_MarkType = {
	PyObject_HEAD_INIT(NULL)
	0,                              /* ob_size */
	"Mark",                         /* tp_name */
	sizeof(mathlink_Mark),          /* tp_basicsize */
	0,                              /* tp_itemsize */
	
	/* Standard Methods */
	(destructor) Mark_Dealloc,      /* tp_dealloc */
	(printfunc) 0,                  /* tp_print */
	(getattrfunc) Mark_Getattr,     /* tp_getattr */
	(setattrfunc) 0,                /* tp_setattr */
	(cmpfunc) 0,                    /* tp_compare */
	(reprfunc) Mark_Repr,           /* tp_repr */
	
	/* Type Categories */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	(hashfunc) 0,                   /* tp_hash */
	(ternaryfunc) 0,                /* tp_call */
	(reprfunc) 0,                   /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,             /* tp_flags */
	"Mark objects",                 /* tp_doc */
};

/* We have to wrap Python's memory allocator and deallocator with these
wrapper functions in order to avoid problems with stdcall and cdecl
calling conventions on Windows. */
MLDEFN(void *, mathlink_MemMallocWrapper, (size_t bytes))
{
	return PyMem_Malloc(bytes);
}

MLDEFN(void, mathlink_MemFreeWrapper, (void *bytes))
{
	PyMem_Free(bytes);
}


/* Module initialization function */
void initmathlink(void)
{
	PyObject *m, *d, *Version;
	
	mathlink_EnvType.ob_type  = &PyType_Type;
	mathlink_LinkType.ob_type = &PyType_Type;
	mathlink_MarkType.ob_type = &PyType_Type;
	
	m = Py_InitModule("mathlink",mathlink_methods);
	d = PyModule_GetDict(m);

	/* Add the encoding types. */
	mathlink_AddMessageCodesToDict(d,mathlink_EncodingCodes,0);

	/* Create the Error Dictionary */
	ErrorDictionary = PyDict_New(); 
	mathlink_AddMessageCodesToDict(ErrorDictionary,mathlink_ErrorMessages,1);
	PyDict_SetItemString(d,"errordictionary",ErrorDictionary);
	mathlink_AddMessageCodesToDict(d,mathlink_ErrorCodes,0);

	/* Create new exception mathlink.error */
	mathlinkError = PyErr_NewException("mathlink.error",NULL,NULL);
	PyDict_SetItemString(d,"error",mathlinkError);
	
	/* Create new exception mathlink.readyparallelerror */
	mathlinkReadyParallelError = PyErr_NewException("mathlink.readyparallelerror",NULL,NULL);
	PyDict_SetItemString(d,"readyparallelerror",mathlinkReadyParallelError);
	
	/* Add the link modes to to the Module Dictionary */
	mathlink_AddMessageCodesToDict(d, linkmodes,0);
	
	/* Create the Token dictionary */
	TokenDictionary = PyDict_New();
	mathlink_AddMessageCodesToDict(TokenDictionary,mathlink_TokenCodes,1);
	PyDict_SetItemString(d,"tokendictionary",TokenDictionary);
	mathlink_AddMessageCodesToDict(d,mathlink_TokenCodes,0);
	
	/* Create the Packet dictionary */
	PacketDictionary = PyDict_New();
	mathlink_AddMessageCodesToDict(PacketDictionary,mathlink_PacketTitle,1);
	PyDict_SetItemString(d,"packetdictionary",PacketDictionary);
	mathlink_AddMessageCodesToDict(d,mathlink_PacketTitle,0);
	
	/* Create the Packet Description Dictionary */
	PacketDescriptionDictionary = PyDict_New();
	mathlink_AddMessageCodesToDict(PacketDescriptionDictionary,mathlink_PacketDescription,1);
	PyDict_SetItemString(d,"packetdescriptiondictionary",PacketDescriptionDictionary);
	
	/* Add the Message Codes to the Module dictionary... */
	MessageCodesDictionary = PyDict_New();
	mathlink_AddMessageCodesToDict(MessageCodesDictionary, mathlink_MessageCodes,1);
	PyDict_SetItemString(d,"messagecodesdictionary",MessageCodesDictionary);
	mathlink_AddMessageCodesToDict(d, mathlink_MessageCodes, 0);
	
	/* Add the Dialog function selectors to the Module dictionary... */
	mathlink_AddMessageCodesToDict(d, mathlink_DialogFunctions, 0);
	
	/* Add the Device Types to the Module dictionary... */
	mathlink_AddMessageCodesToDict(d, mathlink_DeviceTypes, 0);
	
	/* Add the Device Selectors to the Module dictionary... */
	mathlink_AddMessageCodesToDict(d, mathlink_DeviceSelectors,0);
	
	/* Add the pythonlinkversion variable */
	Version = PyString_FromString(PYTHONLINKVERSION);
	if(Version == (PyObject *)NULL){
		PyErr_SetString(PyExc_RuntimeError, "Error setting pythonlinkversion variable");
	}  
	else{
		PyDict_SetItemString(d,"pythonlinkversion",Version);
	}
	
	if(PyErr_Occurred()){
		Py_FatalError("Error initializing mathlink extension module");
	}

}



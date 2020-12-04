#include <atomic>
#include <iostream>
#include <jvmti.h>
#include <ibmjvmti.h>
#include <map>
#include "agentOptions.hpp"
#include "infra.hpp"
#include "json.hpp"
//how many times
using json = nlohmann::json;


/* ENDIAN_HELPERS */

#ifndef SWAP_2BYTES
#define SWAP_2BYTES(v)  (\
						(((v) >> (8*1)) & 0x00FF) | \
						(((v) << (8*1)) & 0xFF00)   \
						)
#endif

#ifndef SWAP_4BYTES
#define SWAP_4BYTES(v) (\
					   (((v) >> (8*3)) & 0x000000FF) | \
					   (((v) >> (8*1)) & 0x0000FF00) | \
					   (((v) << (8*1)) & 0x00FF0000) | \
					   (((v) << (8*3)) & 0xFF000000)   \
					   )
#endif

#ifndef SWAP_8BYTES
#define SWAP_8BYTES(v) (\
						(((v) >> (8*7)) & 0x000000FF) | \
						(((v) >> (8*5)) & 0x0000FF00) | \
						(((v) >> (8*3)) & 0x00FF0000) | \
						(((v) >> (8*1)) & 0xFF000000) | \
						(((v) & 0xFF000000) << (8*1)) | \
						(((v) & 0x00FF0000) << (8*3)) | \
						(((v) & 0x0000FF00) << (8*5)) | \
						(((v) & 0x000000FF) << (8*7)) \
					   )
#endif

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define FORCE_LITTLE_ENDIAN_2BYTES(val) SWAP_2BYTES((val))
#define FORCE_LITTLE_ENDIAN_4BYTES(val) SWAP_4BYTES((val))
#define FORCE_LITTLE_ENDIAN_8BYTES(val) SWAP_8BYTES((U_64)(val))
#else
#define FORCE_LITTLE_ENDIAN_2BYTES(val) (val)
#define FORCE_LITTLE_ENDIAN_4BYTES(val) (val)
#define FORCE_LITTLE_ENDIAN_8BYTES(val) (val)
#endif

#define READ_1BYTE(val)   (val) =  *((U_8*) val;                      dump++;
#define READ_2BYTES(val) *((U_16*)dump) = FORCE_BIG_ENDIAN_2BYTES(val); dump += sizeof(U_16);
#define READ_4BYTES(val) *((U_32*)dump) = FORCE_BIG_ENDIAN_4BYTES((U_32)val); dump += sizeof(U_32);
#define READ_8BYTES(val) *((U_64*)dump) = FORCE_BIG_ENDIAN_8BYTES(val); dump += sizeof(U_64);


struct ClassCycleInfo
{
    int numFirstTier;
    int numSecondTier;
    int numThirdTier;
};
std::atomic<bool> stackTraceEnabled{true};
std::atomic<int> monitorSampleRate{1};
std::atomic<int> monitorSampleCount{0};

void throwError(const char* message){
    fprintf(stderr, "%s\n", message);
    throw("OOPS");
}

void startJlmEvents(jvmtiEnv *jvmti)
{
    jvmtiError err = JVMTI_ERROR_NONE;
    jint extensionFunctionCount = 0;
    jvmtiExtensionFunctionInfo *extensionFunctions = NULL;
    int i = 0;

    err == jvmti->GetExtensionFunctions(&extensionFunctionCount, &extensionFunctions);
    if (err != JVMTI_ERROR_NONE)
    {
        throwError("Could not retrieve extension functions");
    }
    else
    {

        while (i++ < extensionFunctionCount)
        {
            jvmtiExtensionFunction function = extensionFunctions->func;

            if (strcmp(extensionFunctions->id, COM_IBM_SET_VM_JLM) == 0)
            {
                /* found the set vm jlm function, so set it */
                err = function(jvmti, COM_IBM_JLM_START);
                if (err != JVMTI_ERROR_NONE)
                {
                    throwError("Could not enable JLM");
                }
            }
            extensionFunctions++;
        }
    }
    return;
}

void endJlmEvents(jvmtiEnv *jvmti)
{
    jvmtiError err = JVMTI_ERROR_NONE;
    jint extensionFunctionCount = 0;
    jvmtiExtensionFunctionInfo *extensionFunctions = NULL;
    int i = 0;
    int dumpOffset = 8; // because dump version is printed
    jvmti->GetExtensionFunctions(&extensionFunctionCount, &extensionFunctions);
    if (err != JVMTI_ERROR_NONE)
    {
        throwError("Could not retrieve extension functions");
    }
    else
    {
        while (i++ < extensionFunctionCount)
        {
            jvmtiExtensionFunction function = extensionFunctions->func;

            if (strcmp(extensionFunctions->id, COM_IBM_JLM_DUMP_STATS) == 0)
            {
                /* found the setdump stats function */
                jlm_dump *ptr = NULL;
                char* pos = NULL;

                // call for a dump
                err = function(jvmti, &ptr, COM_IBM_JLM_DUMP_FORMAT_OBJECT_ID);
                if (err != JVMTI_ERROR_NONE)
                    throwError("Could not call a jlm dump");

                printf("BEGIN - END: %ld\n", (ptr->end - ptr->begin) - dumpOffset);
                for (int i = 0; i < ptr->end - ptr->begin; i++)
                    printf("%x", ((jlm_dump *)ptr)->begin[i]);
                printf("\n");

                pos = (char*) ptr->begin + dumpOffset;


                if(*((uint8_t*)pos++) == 0x01){
                    printf("JAVA MONITOR\n");
                } else{
                    printf("RAW MONITOR\n");
                }

                uint64_t location = 0;
                location += (*pos++ & 0xFFFFFFFFFFFFFFFFu) << 0;
                location += (*pos++ & 0xFFFFFFFFFFFFFFFFu) << 8;
                location += (*pos++ & 0xFFFFFFFFFFFFFFFFu) << 16;
                location += (*pos++ & 0xFFFFFFFFFFFFFFFFu) << 24;
                location += (*pos++ & 0xFFFFFFFFFFFFFFFFu) << 32;
                location += (*pos++ & 0xFFFFFFFFFFFFFFFFu) << 40;
                location += (*pos++ & 0xFFFFFFFFFFFFFFFFu) << 48;
                location += (*pos   & 0xFFFFFFFFFFFFFFFFu) << 56;
                char* name = (char *) location;
                printf("name: %ld\n", location);

                
                
            }
            extensionFunctions++;
        }
    }
    return;
}

void setMonitorStackTrace(bool val)
{
    // Enables or disables the stack trace option
    stackTraceEnabled = val;
    return;
}

void setMonitorSampleRate(int rate)
{
    if (rate > 0)
    {
        stackTraceEnabled = true;
        monitorSampleRate = rate;
    }
    else
    {
        stackTraceEnabled = false;
    }
}

JNIEXPORT void JNICALL MonitorContendedEntered(jvmtiEnv *jvmtiEnv, JNIEnv *env, jthread thread, jobject object)
{
    startJlmEvents(jvmtiEnv);
    json j;
    jvmtiError error;
    static std::map<const char *, ClassCycleInfo> numContentions;
    jclass cls = env->GetObjectClass(object);
    // First get the class object
    jmethodID mid = env->GetMethodID(cls, "getClass", "()Ljava/lang/Class;");
    jobject clsObj = env->CallObjectMethod(object, mid);
    // Now get the class object's class descriptor
    cls = env->GetObjectClass(clsObj);
    // Find the getName() method on the class object
    mid = env->GetMethodID(cls, "getName", "()Ljava/lang/String;");
    // Call the getName() to get a jstring object back
    jstring strObj = (jstring)env->CallObjectMethod(clsObj, mid);
    // Now get the c string from the java jstring object
    const char *str = env->GetStringUTFChars(strObj, NULL);
    /*char *str;
    error = jvmtiEnv->GetClassSignature(cls , &str, NULL);
    if (str != NULL && error == JVMTI_ERROR_NONE) */
    // record calling class
    j["Class"] = str;

    numContentions[str].numFirstTier++;

    int num = numContentions[str].numFirstTier;
    j["numTypeContentions"] = num;
    // Release the memory pinned char array
    env->ReleaseStringUTFChars(strObj, str);
    env->DeleteLocalRef(cls);

    if (true)
    { // only run if the backtrace is enabled
        if (monitorSampleCount % monitorSampleRate == 0)
        {
            jvmtiFrameInfo frames[5];
            jint count;
            jvmtiError err;
            err = jvmtiEnv->GetStackTrace(thread, 0, 5,
                                          frames, &count);
            if (err == JVMTI_ERROR_NONE && count >= 1)
            {
                char *methodName;
                err = jvmtiEnv->GetMethodName(frames[0].method,
                                              &methodName, NULL, NULL);
                if (err == JVMTI_ERROR_NONE)
                {
                    j["Method"] = methodName;
                }
            }
        }
        monitorSampleCount++;
    }
    // printf("%s\n", j.dump().c_str());
    sendToServer(j.dump());
    endJlmEvents(jvmtiEnv);
}

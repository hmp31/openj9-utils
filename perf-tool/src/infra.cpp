#include <jvmti.h>
#include <ibmjvmti.h>
#include <iostream>
#include <thread>
#include <string.h>

#include "infra.hpp"
#include "server.hpp"

Server *server;

void throwError(const char* message){
    fprintf(stderr, "%s\n", message);
    throw("OOPS");
}



void check_jvmti_error(jvmtiEnv *jvmti, jvmtiError errnum, const char *str)
{
    if (errnum != JVMTI_ERROR_NONE)
    {
        char *errnum_str;
        errnum_str = NULL;
        (void)jvmti->GetErrorName(errnum, &errnum_str);
        printf("ERROR: JVMTI: [%d] %s - %s", errnum, (errnum_str == NULL ? "Unknown" : errnum_str), (str == NULL ? "" : str));
        throw "Oops!";
    }
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
    int dumpOffset = 0; // because dump version is printed
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
                    printf("%c",((jlm_dump *)ptr)->begin[i]);
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

jthread createNewThread(JNIEnv *jni_env)
{
    // allocates a new java thread object using JNI
    jclass threadClass;
    jmethodID id;
    jthread newThread;

    threadClass = jni_env->FindClass("java/lang/Thread");
    id = jni_env->GetMethodID(threadClass, "<init>", "()V");
    newThread = jni_env->NewObject(threadClass, id);
    return newThread;
}

void JNICALL startServer(jvmtiEnv *jvmti, JNIEnv *jni, void *p)
{
    server->handleServer();
}

void sendToServer(std::string message)
{
    server->messageQueue.push(message);
}

JNIEXPORT void JNICALL VMInit(jvmtiEnv *jvmtiEnv, JNIEnv *jni_env, jthread thread)
{
    jvmtiError error;
    int *portPointer = portNo ? &portNo : NULL;

    server = new Server(portNo, commandsPath, logPath);

    error = jvmtiEnv->RunAgentThread(createNewThread(jni_env), &startServer, portPointer, JVMTI_THREAD_NORM_PRIORITY);
    startJlmEvents(jvmtiEnv);
    check_jvmti_error(jvmtiEnv, error, "Error starting agent thread\n");
    printf("VM starting up\n");
}

JNIEXPORT void JNICALL VMDeath(jvmtiEnv *jvmtiEnv, JNIEnv *jni_env)
{
    endJlmEvents(jvmtiEnv);
    server->shutDownServer();
    printf("VM shutting down\n");
}

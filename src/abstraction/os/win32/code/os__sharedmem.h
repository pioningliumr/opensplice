/*
 *                         OpenSplice DDS
 *
 *   This software and documentation are Copyright 2006 to 2009 PrismTech 
 *   Limited and its licensees. All rights reserved. See file:
 *
 *                     $OSPL_HOME/LICENSE 
 *
 *   for full copyright notice and license terms. 
 *
 */

#ifndef OS_WIN32__SHAREDMEM_H
#define OS_WIN32__SHAREDMEM_H

#if defined (__cplusplus)
extern "C" {
#endif

#include <os_mutex.h>

/** \brief Initialize shared memory module
 */
void
os_sharedMemoryInit (
    void);

/** \brief Deinitialize shared memory module
 */
void
os_sharedMemoryExit (
    void);

char *
os_getDomainNameforMutex(
        os_mutex *mutex);

os_address
os_getShmBaseAddressFromPointer(
    void *vpointer);

#if defined (__cplusplus)
}
#endif



#endif /* OS_WIN32__SHAREDMEM_H */

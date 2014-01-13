/**
 *  @file
 *  
 */

#pragma once

#include  "messaging.h"


/**
 *  Initialize our separate message display window.  Unless requested,
 *  this window is never visible.  But we always allocate it to make
 *  sure it is available when needed.
 */
void  message_window_init(void);

void  message_window_deinit(void);


/**
 *  Write the supplied error info to our message window, and make sure
 *  it is visible.
 *  
 *  @param pszCaption Points to text to show in window caption.
 *                Supplied text must exist for life of message window.
 *  @param pszText Points to text to show in window body.
 *                Supplied text must exist for life of message window.
 */
void  message_window_show_status (const char *pszCaption, const char *pszText);

/**
 *  Write the supplied error info to our message window, and make sure
 *  it is visible.
 * 
 *  @param eErrSrc 
 *  @param errCode 
 *  @param pszErrMsg 
 */
void  message_window_show_error (FailureSource eErrSrc,
                                 int32_t errCode, const char *pszErrMsg);


/**
 *  Hide our message window, if it was displayed.
 */
void  message_window_hide();



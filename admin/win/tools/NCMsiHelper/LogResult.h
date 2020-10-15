/*
 * Copyright (C) by Michael Schuster <michael@schuster.ms>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 * 
 * Parts of this file are based on:
 * https://www.codeproject.com/articles/570751/devmsi-an-example-cplusplus-msi-wix-deferred-custo
 * 
 * Licensed under the The Code Project Open License (CPOL):
 * https://www.codeproject.com/info/cpol10.aspx
 * 
 */

/**
 *  Function prototype for LogResult()
 */
#pragma once

/**
 *  Log a message.
 *
 *  If the DLL is being used in a WiX MSI environment, LogResult() will
 *  route any log messages to the MSI log file via WcaLog() or WcaLogError().
 *
 *  If the DLL is NOT being used in a WiX MSI environment, LogResult() will
 *  route any log messages to stdout or stderr.
 *
 *  If the result is an error code, LogResult will attempt to gather a 
 *  text version of the error code and place it in the log.  For example,
 *  if the error code means ERROR_FILE_NOT_FOUND, it will look up the appropriate
 *  message ( via FormatMessage() ) and add "The system cannot find the file specified."
 *  to the log.
 *
 * @param hr The HRESULT to be interrogated for success or failure.
 * @param fmt The string format for a user-specified error message.
 */
void LogResult(
    __in HRESULT hr,
    __in_z __format_string PCSTR fmt, ...
);

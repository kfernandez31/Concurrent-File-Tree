#pragma once

/* wypisuje informacje o błędnym zakończeniu funkcji systemowej
i kończy działanie */
extern void syserr(const char* fmt, ...);

/* wypisuje informacje o błędzie i kończy działanie */
extern void fatal(const char* fmt, ...);

/**
 * Checks whether the caller's result was success.
 * Calls `syserr` with the caller's error code if one occurred.
 * @param res : function call result
 * @param caller : calling function name
 */
/*void err_check(int res, const char *caller);*/
//TODO: wywal
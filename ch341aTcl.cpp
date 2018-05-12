#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define USE_TCL_STUBS
extern "C" {
  #include <tcl.h>
  #include "ch341a.h"
}

extern "C" int verbose;
int verbose = 1;

static bool init = false;

static int MyCmd(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj* const* objv);
static void FreeCh341a(ClientData cdata);

static int InitCmd(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj* const* objv) {
  const char* cmdname;
  uint16_t usb_vid = CH341A_USB_VENDOR, usb_pid = CH341A_USB_PRODUCT;
  uint32_t speed = CH341A_STM_I2C_20K;

  if (init) {
    Tcl_SetResult(interp, (char*)"The library only supports a single open device at a time.", TCL_STATIC);
    return TCL_ERROR;
  }

  //TODO support more arguments
  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "cmdname");
    return TCL_ERROR;
  }
  cmdname = Tcl_GetString(objv[1]);

  int ret = ch341Configure(usb_vid, usb_pid);
  if (ret < 0) {
    Tcl_SetResult(interp, (char*)"ch341Configure failed. Make sure the device is connected.", TCL_STATIC);
    return TCL_ERROR;
  }

  ret = ch341SetStream(speed);
  if (ret < 0) {
    Tcl_SetResult(interp, (char*)"ch341SetStream failed. Make sure that the speed is supported.", TCL_STATIC);
    return TCL_ERROR;
  }

  Tcl_CreateObjCommand(interp, cmdname, MyCmd, NULL, FreeCh341a);
  init = true;

  Tcl_ResetResult(interp);
  return TCL_OK;
}

static void FreeCh341a(ClientData cdata) {
  if (init) {
    ch341Release();
  }
  init = false;
}

static int MyCmd(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj* const* objv) {
  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "subcmd ...");
    return TCL_ERROR;
  }

  int32_t res;
  const char* cmd = Tcl_GetString(objv[1]);

  if (strcmp(cmd, "raw") == 0) {
    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 2, objv, "data");
      return TCL_ERROR;
    }

    int len;
    char* data = Tcl_GetStringFromObj(objv[2], &len);
    char* out = (char*)Tcl_Alloc(len+1);
    res = ch341SpiStream((uint8_t*)out, (uint8_t*)data, (uint32_t)len);
    if (res < 0) {
      Tcl_SetResult(interp, (char*)"error in ch341SpiStream", TCL_STATIC);
      return TCL_ERROR;
    } else {
      out[len] = 0;
      Tcl_SetObjResult(interp, Tcl_NewStringObj(out, len));
      Tcl_Free(out);
      return TCL_OK;
    }
  } else if (strcmp(cmd, "capacity") == 0) {
    if (objc != 2) {
      Tcl_WrongNumArgs(interp, 2, objv, "");
      return TCL_ERROR;
    }

    int32_t cap = ch341SpiCapacity();
    if (cap < 0) {
      Tcl_SetResult(interp, (char*)"error in ch341SpiCapacity", TCL_STATIC);
      return TCL_ERROR;
    } else {
      Tcl_SetObjResult(interp, Tcl_NewIntObj(cap));
      return TCL_OK;
    }
  } else if (strcmp(cmd, "status") == 0) {
    if (objc != 2 && objc != 3) {
      Tcl_WrongNumArgs(interp, 2, objv, "[status]");
      return TCL_ERROR;
    }

    if (objc == 2) {
      int32_t status = ch341ReadStatus();
      if (status < 0) {
        Tcl_SetResult(interp, (char*)"error in ch341ReadStatus", TCL_STATIC);
        return TCL_ERROR;
      } else {
        Tcl_SetObjResult(interp, Tcl_NewIntObj(status));
        return TCL_OK;
      }
    } else {
      int statusArg;
      if (Tcl_GetIntFromObj(interp, objv[2], &statusArg) != TCL_OK)
        return TCL_ERROR;
      if (statusArg < 0 || statusArg > 255) {
        Tcl_SetResult(interp, (char*)"status must be uint8_t (0 to 255)", TCL_STATIC);
        return TCL_ERROR;
      }

      res = ch341WriteStatus((uint8_t)statusArg);
      if (res < 0) {
        Tcl_SetResult(interp, (char*)"error in ch341WriteStatus", TCL_STATIC);
        return TCL_ERROR;
      } else {
        Tcl_ResetResult(interp);
        return TCL_OK;
      }
    }
  } else if (strcmp(cmd, "erase") == 0) {
    if (objc != 2) {
      Tcl_WrongNumArgs(interp, 2, objv, "");
      return TCL_ERROR;
    }

    res = ch341EraseChip();
    if (res < 0) {
      Tcl_SetResult(interp, (char*)"error in ch341EraseChip", TCL_STATIC);
      return TCL_ERROR;
    } else {
      Tcl_ResetResult(interp);
      return TCL_OK;
    }
  } else if (strcmp(cmd, "read") == 0) {
    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 2, objv, "addr length");
      return TCL_ERROR;
    }

    int addr, length;
    if (Tcl_GetIntFromObj(interp, objv[2], &addr) != TCL_OK)
      return TCL_ERROR;
    if (Tcl_GetIntFromObj(interp, objv[3], &length) != TCL_OK)
      return TCL_ERROR;
    if (addr < 0 || length < 0) {
      Tcl_SetResult(interp, (char*)"address and length must be non-negative", TCL_STATIC);
      return TCL_ERROR;
    }

    char* buf = Tcl_Alloc(length+1);
    res = ch341SpiRead((uint8_t*)buf, addr, length);
    if (res < 0) {
      Tcl_SetResult(interp, (char*)"error in ch341SpiRead", TCL_STATIC);
      return TCL_ERROR;
    } else {
      buf[length] = 0;
      Tcl_SetObjResult(interp, Tcl_NewStringObj(buf, length));
      Tcl_Free(buf);
      return TCL_OK;
    }
  } else if (strcmp(cmd, "write") == 0) {
    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 2, objv, "addr data");
      return TCL_ERROR;
    }

    int addr, length;
    if (Tcl_GetIntFromObj(interp, objv[2], &addr) != TCL_OK)
      return TCL_ERROR;
    char* data = Tcl_GetStringFromObj(objv[3], &length);
    if (addr < 0) {
      Tcl_SetResult(interp, (char*)"address and length must be non-negative", TCL_STATIC);
      return TCL_ERROR;
    }

    res = ch341SpiWrite((uint8_t*)data, addr, length);
    if (res < 0) {
      Tcl_SetResult(interp, (char*)"error in ch341SpiWrite", TCL_STATIC);
      return TCL_ERROR;
    } else {
      Tcl_ResetResult(interp);
      return TCL_OK;
    }
  } else if (strcmp(cmd, "verbose") == 0) {
    if (objc != 2 && objc != 3) {
      Tcl_WrongNumArgs(interp, 2, objv, "[verbose]");
      return TCL_ERROR;
    }

    if (objc == 2) {
      Tcl_SetObjResult(interp, Tcl_NewIntObj(verbose));
      return TCL_OK;
    } else {
      int newVerbose;
      if (Tcl_GetIntFromObj(interp, objv[2], &newVerbose) != TCL_OK)
        return TCL_ERROR;
      verbose = newVerbose;
      Tcl_ResetResult(interp);
      return TCL_OK;
    }
  } else {
    Tcl_SetResult(interp, (char*)"invalid command. supported commands are capacity, status, erase, read, write, raw, verbose", TCL_STATIC);
    return TCL_ERROR;
  }
}

extern "C" int DLLEXPORT Ch341a_Init(Tcl_Interp* interp);
int DLLEXPORT Ch341a_Init(Tcl_Interp* interp) {
  if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL)
    return TCL_ERROR;
  if (Tcl_PkgProvide(interp, "ch341a", "1.0") == TCL_ERROR)
    return TCL_ERROR;
  Tcl_CreateObjCommand(interp, "ch341a", InitCmd, NULL, NULL);
  return TCL_OK;
}

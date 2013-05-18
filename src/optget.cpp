#include <string.h>
#include <stdlib.h>
#include "optget.h"


int optget_parse(OptGetItem* items, char* const* args[], OptGetItem** get)
{
  char* val;

  // Last arg
  if(**args == NULL) {
    *get = NULL;
    return OPTGET_LAST;
  }

  //XXX Skip "--"
  while(!strcmp(**args, "--")) {
    (*args)++;
  }

  // Extra arg (not an option)
  if(!strcmp(**args, "-") || (**args)[0] != '-') {
    for(*get=items; (*get)->type != OPTGET_NONE; (*get)++) ;
    (*get)->value.str = **args;
    (*args)++;
    return OPTGET_OK;
  }

  if((**args)[1] == '-') {
    // Long name
    for(*get=items; (*get)->type != OPTGET_NONE; (*get)++) {
      if(!strcmp((**args)+2, (*get)->long_name)) {
        break;
      }
    }
    // Unknown option
    if((*get)->type == OPTGET_NONE) {
      *get = NULL;
      return OPTGET_ERR_LONG_NAME;
    }
    val = *++(*args); // value (if any): next argument
  } else {
    // Short name
    for(*get=items; (*get)->type != OPTGET_NONE; (*get)++) {
      if((**args)[1] == (*get)->short_name) {
        break;
      }
    }
    // Unknown option
    if((*get)->type == OPTGET_NONE) {
      *get = NULL;
      return OPTGET_ERR_SHORT_NAME;
    }

    if((**args)[2] == '\0') { // separated option name and value
      val = *++(*args);
    } else { // value immediately follows option name
      // Check for unexpected value
      if((*get)->type == OPTGET_FLAG) {
        *get = NULL;
        return OPTGET_ERR_VAL_UNEXP;
      }
      val = &((**args)[2]);
    }
  }

  // Parse/set option value (if any)
  int ret = optget_parse_arg(*get, val);
  if(ret == 0 && (*get)->type != OPTGET_FLAG) {
    (*args)++;
  }

  return ret;
}


int optget_parse_arg(OptGetItem* item, char* arg)
{
  // Parse/set option value (if any)
  switch(item->type) {
    case OPTGET_STR: {
      if(arg == NULL) {
        return OPTGET_ERR_VAL_MISSING;
      }
      item->value.str = arg;
    } break;

    case OPTGET_INT: {
      if(arg == NULL) {
        return OPTGET_ERR_VAL_MISSING;
      }
      char* arg2;
      long l = strtol(arg, &arg2, 0);
      if(arg2 != arg+strlen(arg)) { // conversion failed
        return OPTGET_ERR_VAL_FMT;
      }
      item->value.i = l;
    } break;

    case OPTGET_NONE:
    case OPTGET_FLAG:
    default:
      break;
  }

  return OPTGET_OK;
}


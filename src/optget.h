#ifndef OPTGET_H_
#define OPTGET_H_


/** @brief OptGet option types
 *
 * @note Integer parsing accept decimal, octal and hexadecimal constants.
 */
typedef enum
{
  OPTGET_NONE,
  OPTGET_FLAG,   ///< Flag (no value)
  OPTGET_STR,    ///< String argument
  OPTGET_INT,    ///< Integer argument (signed, long)

} OptGetType;


/// OptGet option description
typedef struct
{
  const char short_name;  ///< Short option name (0 if none)
  const char* long_name;  ///< Long option name (NULL if none)
  OptGetType type;  ///< Option type

  /// Parsed value
  union {
    char* str;  ///< String
    long i;  ///< Integer argument
  } value;

} OptGetItem;


/// Return values of parsing function
enum OptGetRetValues_
{
  OPTGET_OK              =  0, ///< Function succeded
  OPTGET_LAST            =  1, ///< Last argument, nothing parsed
  OPTGET_ERR_SHORT_NAME  = -1, ///< Unknown short option name
  OPTGET_ERR_LONG_NAME   = -2, ///< Unknown long option name
  OPTGET_ERR_VAL_FMT     = -3, ///< Invalid option value format
  OPTGET_ERR_VAL_MISSING = -4, ///< Missing option value
  OPTGET_ERR_VAL_UNEXP   = -5, ///< Unexpected option value
};


/** @brief Parse arguments using given options.
 *
 * The last item type must be \e OPTGET_NONE. It is used to locate item array
 * end and for extra arguments.
 * 
 * Argument pointer pointed by \e args is incremented by the function to point
 * to the next argument to parse. If an error occurs, it is set to point the
 * invalid argument (option name or value).
 *
 * @param  items  array of items ended by an \e OPTGET_NONE item
 * @param  args   pointer to parsed arguments, incremented by the function
 * @param  get    set to parsed option
 *
 * @sa OptGetRetValues_ for return values.
 *
 * @todo Properly process "--".
 */
int optget_parse(OptGetItem* items, char* const* args[], OptGetItem** get);


/** @brief Parse a single argument.
 *
 * This method can be used to manually parse an extra argument to a given type.
 *
 * @return OPTGET_OK on success, OPTGET_ERR_VAL_FMT on error.
 */
int optget_parse_arg(OptGetItem* get, char* arg);


#endif

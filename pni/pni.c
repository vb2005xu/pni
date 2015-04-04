/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2014 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Zuocheng Liu <zuocheng.liu@gmail.com>                        |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_pni.h"
#include <dlfcn.h>

ZEND_BEGIN_ARG_INFO_EX(arginfo_pni___call, 0, 0, 2)
     ZEND_ARG_INFO(0, function_name)
     ZEND_ARG_INFO(0, arguments)
ZEND_END_ARG_INFO()


/* If you declare any globals in php_pni.h uncomment this:*/
ZEND_DECLARE_MODULE_GLOBALS(pni)
/* True global resources - no need for thread safety here */
static int le_dl_handle_persist;


/* Local functions */
static void php_dl_handle_persist_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC);


static zend_class_entry *pni_ptr;
static zend_class_entry *pni_exception_ptr;

/* PNI functions */
PHP_FUNCTION(get_pni_version);
PHP_METHOD(PNI, __construct);                                      
PHP_METHOD(PNI, __call);

/* {{{ pni_functions[]
 *
 * Every user visible function must have an entry in pni_functions[].
 */
const zend_function_entry pni_functions[] = {
    PHP_FE(get_pni_version,    NULL) 
    
    PHP_ME(PNI, __construct,     NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR) 
    PHP_ME(PNI, __call,     arginfo_pni___call, 0) 
    
    PHP_FE_END  /* Must be the last line in pni_functions[] */
};
/* }}} */

/* {{{ pni_module_entry
 */
zend_module_entry pni_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "pni",
    pni_functions,
    PHP_MINIT(pni),
    PHP_MSHUTDOWN(pni),
    PHP_RINIT(pni),     /* Replace with NULL if there's nothing to do at request start */
    PHP_RSHUTDOWN(pni), /* Replace with NULL if there's nothing to do at request end */
    PHP_MINFO(pni),
#if ZEND_MODULE_API_NO >= 20010901
    PHP_PNI_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PNI
ZEND_GET_MODULE(pni)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("pni.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_pni_globals, pni_globals)
    STD_PHP_INI_ENTRY("pni.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_pni_globals, pni_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_pni_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_pni_init_globals(zend_pni_globals *pni_globals)
{
    pni_globals->global_value = 0;
    pni_globals->global_string = NULL;
}
*/
/* }}} */







/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(pni) {
    zend_class_entry pni; 
    /* If you have INI entries, uncomment these lines 
    REGISTER_INI_ENTRIES();
    */
    le_dl_handle_persist = zend_register_list_destructors_ex(NULL, php_dl_handle_persist_dtor, PHP_DL_HANDLE_RES_NAME, module_number);
    
    INIT_CLASS_ENTRY(pni, "PNI", pni_functions);
    pni_ptr = zend_register_internal_class_ex(&pni, NULL, NULL TSRMLS_CC);
    
    INIT_CLASS_ENTRY(pni, "PNIException", pni_functions);
    pni_exception_ptr = zend_register_internal_class_ex(&pni, zend_exception_get_default(TSRMLS_C), NULL TSRMLS_CC);
    

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(pni) {
    /* uncomment this line if you have INI entries
    UNREGISTER_INI_ENTRIES();
    */
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(pni)
{
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(pni)
{
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(pni)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "pni support", "enabled");
    php_info_print_table_end();

    /* Remove comments if you have entries in php.ini
    DISPLAY_INI_ENTRIES();
    */
}
/* }}} */


/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_pni_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(get_pni_version) {
    RETURN_STRING(PHP_PNI_VERSION, 1);
}

/* {{{ proto public void PNI::__construct($libName)
 *    Constructor. Throws an Exception in case the given shared library does not exist */
PHP_METHOD(PNI, __construct) {
    char *libName = NULL;
    int libNameLen = 0;
    zval *zendValue = NULL, *self = NULL;
   
    char *key = NULL;
    char * error_msg = NULL;
    int key_len = 0;
    void * dlHandle = NULL;
    zend_rsrc_list_entry *le, new_le;
    
    /* get param $libName */
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &libName, &libNameLen) == FAILURE) {
        WRONG_PARAM_COUNT;
    }

 
    /* get the dl handle, if not exists, create it and persist it */
    key_len = spprintf(&key, 0, "pni_dl_handle_%s\n", libName);
    if (zend_hash_find(&EG(persistent_list), key, key_len + 1, (void **)&le) == SUCCESS) {
        ZEND_REGISTER_RESOURCE(return_value, le->ptr, le_dl_handle_persist);
        efree(key);
        RETURN_TRUE;
    }
    
    /* init the dl handle resource */
    dlHandle = dlopen(libName, RTLD_LAZY);
    if(!dlHandle) {
        //php_error_docref(NULL TSRMLS_CC, E_WARNING, "dlopen error (%s) , dl handle resource not created.", dlerror());
        //RETURN_FALSE;
        spprintf(&error_msg, 0, "Dlopen %s error (%s),  dl handle resource is not created.", libName, dlerror());
        zend_throw_exception(pni_exception_ptr, error_msg, 0 TSRMLS_CC);
        RETURN_FALSE;
    }
    /* registe dl handle resource */
    ZEND_REGISTER_RESOURCE(return_value, &dlHandle, le_dl_handle_persist);
    /* persist dl handle */
    new_le.ptr = dlHandle;
    new_le.type = le_dl_handle_persist;
    zend_hash_add(&EG(persistent_list), key, key_len + 1, &new_le, sizeof(zend_rsrc_list_entry), NULL);
    efree(key);
    /* save the libname to private variable */
    self = getThis();
    MAKE_STD_ZVAL(zendValue);
    ZVAL_STRINGL(zendValue, libName, libNameLen, 0);
    SEPARATE_ZVAL_TO_MAKE_IS_REF(&zendValue);
    zend_update_property(Z_OBJCE_P(self), self, ZEND_STRL("_libName"), zendValue TSRMLS_CC);
    RETURN_TRUE;
}
/* }}} */


/* {{{ proto public void PNI::__call($functionName, $args)
Returns a zval pointer */
PHP_METHOD(PNI, __call) {
    zval *self,  *libName, *args, *res;
    zval **data;
    HashTable *arrHash;
    HashPosition pointer;
    char * functionName;
    char * error_msg;
    int i = 0, arrayCount = 0;
    int functionNameLen = 0;
    zval *argList[MAX_PNI_FUNCTION_PARAMS];
    NATIVE_INTERFACE nativeInterface = NULL;
    
    char *key = NULL;
    int key_len = 0;
    void * dlHandle = NULL;
    zend_rsrc_list_entry *le, new_le;
    

    self = getThis();
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &functionName, &functionNameLen, &args) == FAILURE) {
        WRONG_PARAM_COUNT;
    }
    libName = zend_read_property(Z_OBJCE_P(self), self, ZEND_STRL("_libName"), 0 TSRMLS_CC);
    arrHash = Z_ARRVAL_P(args);
    arrayCount = zend_hash_num_elements(arrHash);
    for(zend_hash_internal_pointer_reset_ex(arrHash, &pointer);
            zend_hash_get_current_data_ex(arrHash, (void**) &data, &pointer) == SUCCESS;
            zend_hash_move_forward_ex(arrHash, &pointer)) {
        argList[i] = *data;
        i++;
    }
    key_len = spprintf(&key, 0, "pni_dl_handle_%s\n", Z_STRVAL_P(libName));
    if (zend_hash_find(&EG(persistent_list), key, key_len + 1, (void **)&le) == SUCCESS) {
        ZEND_REGISTER_RESOURCE(return_value, le->ptr, le_dl_handle_persist);
        efree(key);
    }
    dlHandle = le->ptr;
    if(!dlHandle) {
        spprintf(&error_msg, 0, "Fail to all Native Interface. The PNI dl handle (%s) is invalid.", Z_STRVAL_P(libName));
        zend_throw_exception(pni_exception_ptr, error_msg, 0 TSRMLS_CC);
        RETURN_FALSE;
    }

    nativeInterface = (NATIVE_INTERFACE)dlsym(dlHandle, functionName);
    if (!nativeInterface) {
        spprintf(&error_msg, 0, "Dlsym %s error (%s). ", functionName, dlerror());
        zend_throw_exception(pni_exception_ptr, error_msg, 0 TSRMLS_CC);
        RETURN_FALSE;
    }
    res = nativeInterface(argList);
    RETURN_ZVAL(res, 1, 0);
}


/* release the dl resource*/
static void php_dl_handle_persist_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) {
    void *dlHandle = (void *) rsrc->ptr;
    dlclose(dlHandle);
}


/* }}} */

/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
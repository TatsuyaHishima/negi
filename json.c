#include <jansson.h>

json_t json_add_str_str(json_t *json_file, char *string, char *string2) {
	// char* string = "address";
	// char* string2 = "192.168.10.4";
	// int integer = 24;
	// int i;

    /* Make json data */
    // json_t *obj = json_object(), *count_obj = json_object();

    json_object_set_new(json_file, string, json_string(string2));

    // for(i = 0; i < 2; i++) {
    //     char buf[8];
    //     sprintf(buf, "%d", i);
    //     json_object_set(count_obj, buf, json_integer(integer));
      
    // }
    // json_object_set_new(obj, "netmask", count_obj);

    // convert to a json string
    char *json_data = json_dumps(obj, JSON_INDENT(4));
    // json_decref(obj); 
    printf("%s\n", json_data);

    // const char *parse_address = json_string_value(json_object_get(obj, "address"));
    // printf("\"address\" is %s\n", parse_address);

    // free(parse_address);
    free(json_data);

    return json_file;
}

json_t json_add_str_int(json_t *json_file, char *string, int integer) {
    json_object_set_new(json_file, string, json_integer(integer));
    return json_file;
}
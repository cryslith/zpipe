int receive_stdin(int *cont, int *close_zephyr);
Code_t receive_zwrite(int *err);
Code_t zwrite(unsigned short charset,
              char *sender, char *klass, char *instance,
              char *recipient, char *opcode,
              int message_len, char *message, int auth);

Code_t receive_subscription(int *cont, int unsub);
Code_t subscribe(char *klass, char *instance, char *recipient);
Code_t unsubscribe(char *klass, char *instance, char *recipient);

Code_t receive_notice(void);
Code_t process_notice(ZNotice_t *notice, struct sockaddr_in *from);

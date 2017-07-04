int receive_stdin(int *cont, int *close_zephyr);
Code_t receive_zwrite(int *err);
Code_t zwrite(unsigned short charset,
              char *sender, char *klass, char *instance,
              char *recipient, char *opcode,
              int message_len, char *message, int auth);

Code_t receive_subscription(int *cont, int unsub);
Code_t subscribe(char *klass, char *instance, char *recipient, int should_sub,
                 int *success);
Code_t ensure_subscription(char *klass, char *instance, char *recipient,
                           int *should_have);
Code_t debug_subscriptions();

Code_t receive_notice(void);
Code_t process_notice(ZNotice_t *notice, struct sockaddr_in *from);

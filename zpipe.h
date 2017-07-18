Code_t write_zerr(Code_t err, char *operation);

int receive_stdin(int *close_zephyr);
void receive_zwrite(void);
Code_t zwrite(unsigned short charset,
              char *sender, char *klass, char *instance,
              char *recipient, char *opcode,
              int message_len, char *message, int auth);

void receive_subscription(int unsub);
Code_t subscribe(char *klass, char *instance, char *recipient, int should_sub,
                 int *success);
Code_t ensure_subscription(char *klass, char *instance, char *recipient,
                           int *should_have);
void debug_subscriptions();

Code_t receive_notice(void);
void process_notice(ZNotice_t *notice, struct sockaddr_in *from);

void send_err(char *operation, const char *err);

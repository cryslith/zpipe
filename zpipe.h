int zwrite(char *sender, char *klass, char *instance,
           char *recipient, char *opcode,
           int message_len, char *message, int auth);

int zwrite_listen(int *err);

int stdin_listen(void);

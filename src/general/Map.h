struct Map;

struct Map *Map(char *const name, int (*const compare)(const void *, const void *));
void Map_(struct Map **m_ptr);
void MapSetDestructor(struct Map *m, void (*const destructor)(void *, void *));
int MapEnsureCapacity(struct Map *m, const int min_capacity);
void MapTrimToSize(struct Map *m);
void MapSort(struct Map *m);
int MapIterate(struct Map *m, const void **const key_ptr, void **value);
int MapSize(const struct Map *m);
int MapIsEmpty(const struct Map *m);
int MapContainsKey(struct Map *m, const void *key);
void *MapGet(struct Map *m, const void *key);
int MapPut(struct Map *m, const void *key, const void *value);
void *MapRemove(struct Map *m, const void *key);
void MapClear(struct Map *m);
void *MapGetOrDefault(struct Map *m, const void *key, const void *defaultValue);
void MapForEach(const struct Map *m, void (*const action)(const void *, void *));
int MapIsEach(const struct Map *m, int (*const predicate)(void *const, void *));
void MapReplaceAll(struct Map *m, int (*const predicate)(void *const, void *), void *(*const replace)(void *const, void *));
void MapRemoveIf(struct Map *m, int (* const filter)(void *const, void *));

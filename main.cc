#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/* Constants */
static const char *store_name = "our store";
static const size_t LINE_LENGTH = 40;
static const unsigned long SERVER_PORT = 27717;
static const size_t MAX_EOF = 10;
#define MY_DEBUG 0 /* set to true to get debug mode (prints addresses */

extern "C" void __cxa_pure_virtual() { exit(1); }

/* Types */
class CartItem {
public:
  CartItem() : quantity(1) { }

  int quantity;
  virtual const char *getName() = 0;

  void *operator new(size_t);
  void operator delete(void *, size_t);

  virtual ~CartItem() { }
};

/* Force malloc implemetation of new */
void *CartItem::operator new(size_t sz) {
  void *res = malloc(sz);
  if (!res) exit(0);
  return res;
}

void CartItem::operator delete(void *p, size_t sz) {
  free(p);
}

#define NEW_ITEM(x) \
  class x: public CartItem { \
  public: \
    const char *getName() { \
      return #x; \
    } \
  };

NEW_ITEM(Apple);
NEW_ITEM(Pear);
NEW_ITEM(Orange);

/* static vars */
static char **notes = NULL;
static size_t num_notes = 0;

static CartItem **items = NULL;
static size_t num_items = 0;

static CartItem *favorite_item = NULL;

static int can_use_admin = 0;
static FILE *in;
static FILE *out;

/* General Functions */
static void *xrealloc(void *ptr, size_t sz) {
  void *res = realloc(ptr, sz);
  if (!res && sz) exit(EXIT_FAILURE);
  return res;
}

static char *xstrdup(const char *in) {
  char *res = strdup(in);
  if (!res) exit(EXIT_FAILURE);
  return res;
}

static char *myfgets(char *c, size_t n) {
  for (size_t i = 0,b = 0; i < n && b < MAX_EOF; i++) {
    int cha = fgetc(in);
    if (cha == EOF) {
      b++;
      i--;
      continue;
    }
    c[i] = (char)cha;
    if (c[i] == '\n') break;
  }
  fprintf(out, "\n");
  return c;
}

static void remove_newline(char *lineptr) {
  char *inside = strchr(lineptr, '\n');
  if (inside)
    *inside = '\0';
}

static int do_selection(const char *lead,
    const char * const * options, const char *prompt) {
  char lineptr[32] = {0};
  int input = -0;
  int z = 0;
  size_t i = ~0;

  if (lead)
    fprintf(out, "%s:\n", lead);

  if (options) {
    for (i = 0; options[i] != NULL; i++) {
      fprintf(out, "  [%lu]: %s\n", i, options[i]);
    }
  }
  if (prompt)
    fprintf(out, "%s:", prompt);

  fflush(out);

  myfgets(lineptr, sizeof(lineptr)-1);

  z = sscanf(lineptr, "%d", &input);

  if (z <= 0 ||
      (i != (size_t)~0 && (input < 0 || (size_t)input >= i))) {
    if (feof(in)) exit(1);
    return do_selection(lead, options, prompt);
  } else {
    return input;
  }
}

static void print_line(char c) {
  for (size_t i = 0; i < LINE_LENGTH; i++) {
    fputc(c, out);
  }
  fputc('\n', out);
}

static void submit_order() {
  fprintf(out, "Your order has been placed.\n");
  fprintf(out, "Thanks for shopping at %s!\n", store_name);
  return;
}

/* Prints */
static void print_cart(int numed) {
  if (num_items == 0) {
    fprintf(out, "Cart is empty.\n\n");
    return;
  }
  fprintf(out, "Contents of cart (%lu):\n", num_items);
  for (size_t i = 0; i < num_items; i++) {
    if (numed) {
      fprintf(out, "  [%lu] ", i);
    }
#if MY_DEBUG
    fprintf(out, "%dx %s %p\n", items[i]->quantity, items[i]->getName(), items[i]);
#else
    fprintf(out, "%dx %s\n", items[i]->quantity, items[i]->getName());
#endif
  }
  fprintf(out, "\n");
}

static void print_notes() {
  if (num_notes == 0) {
    fprintf(out, "There are no notes.\n\n");
    return;
  }
  fprintf(out, "Notes (%lu):\n", num_notes);
  for (size_t i = 0; i < num_notes; i++) {
#if MY_DEBUG
    fprintf(out, "  #%lu: %s %p\n", i, notes[i], notes[i]);
#else
    fprintf(out, "  #%lu: %s\n", i, notes[i]);
#endif
  }
  fprintf(out, "\n");
}

static void print_fav_item() {
  if (favorite_item) {
    fprintf(out, "Your favorite item is a %s\n\n", favorite_item->getName());
  } else {
    fprintf(out, "You don't have a favorite item\n\n");
  }
}

static size_t get_num(const char *prompt, int items) {
  if (items)
    print_cart(1);
  else
    print_notes();

  int i = do_selection(NULL, NULL, prompt);

  if (i < 0 || (size_t)i >= (items ? num_items : num_notes)) {
    return get_num(prompt, items);
  }

  return (size_t)i;
}

/* Cart */
#define MUST_HAVE_ITEMS() \
  do { if (num_items == 0) { \
    fprintf(out, "You have no items\n"); \
    return; \
  } } while(0)

static CartItem *choose_item() {
  const char *opts[] = {"Apples", "Pears", NULL};

  switch(do_selection("Items Available", opts,
    "Which item would you like to add to your cart?")) {
  case 0:
    return new Apple();
  case 1:
    return new Pear();
  default:
    return NULL;
  }
}

static void add_item() {
  CartItem *c;
  if (!(c = choose_item())) return;

  items = (CartItem **)xrealloc(items, (++num_items * sizeof(CartItem**)));

  items[num_items-1] = c;
}

static void set_fav_item() {
  MUST_HAVE_ITEMS();

  favorite_item = items[get_num("Choose your favorite item", 1)];
}

static void change_fav_item() {
  if (!favorite_item) {
    fputs("You don't have a favorite item!\n", out);
    return;
  }

  CartItem *c;
  if (!(c = choose_item())) return;

  memcpy(favorite_item, c, sizeof(CartItem));
  delete c;
}

static void delete_item() {
  MUST_HAVE_ITEMS();

  size_t i = get_num("Choose an item to delete", 1);

  delete items[i];

  num_items--;

  if (i != num_items)
    memmove(&items[i], &items[i+1], (num_items - i) * sizeof(CartItem *));

  items = (CartItem **)xrealloc(items, num_items * sizeof(CartItem *));
}

static void change_item() {
  MUST_HAVE_ITEMS();

  size_t i = get_num("Choose an item to edit", 1);

  items[i]->quantity = do_selection(NULL, NULL, "Enter the new quantity");
}

/* Notes */
#define MUST_HAVE_NOTES() \
  do { if (num_notes == 0) { \
    fprintf(out, "You have no notes\n"); \
    return; \
  } } while (0)

static void get_note() {
  char lineptr[64] = {0};

  fputs("Enter a note:\n", out);
  myfgets(lineptr, sizeof(lineptr)-1);
  remove_newline(lineptr);

  notes = (char**)xrealloc(notes, (++num_notes * sizeof(char *)));
  notes[num_notes - 1] = xstrdup(lineptr);
}

static void read_note_from_file() {
  char lineptr[64] = {0};

  fputs("File name to read from:\n", out);
  myfgets(lineptr, sizeof(lineptr)-1);
  remove_newline(lineptr);

  FILE *x = fopen(lineptr, "r");
  fgets(lineptr, sizeof(lineptr)-1, x);
  fclose(x);

  remove_newline(lineptr);

  notes = (char **)xrealloc(notes, (++num_notes * sizeof(char *)));
  notes[num_notes - 1] = xstrdup(lineptr);
}

static void delete_note() {
  MUST_HAVE_NOTES();

  size_t i = get_num("Choose a note to delete", 0);

  free(notes[i]);

  num_notes--;

  if (i != num_notes)
    memmove(&notes[i], &notes[i+1], (num_notes - i) * sizeof(char *));

  notes = (char **)xrealloc(notes, num_notes * sizeof(char *));
}

static void change_note() {
  MUST_HAVE_NOTES();

  char lineptr[64] = {0};

  size_t i = get_num("Choose a note to edit", 0);

  fputs("Enter the new note:\n", out);
  myfgets(lineptr, sizeof(lineptr)-1);
  remove_newline(lineptr);

  free(notes[i]);
  notes[i] = xstrdup(lineptr);
}

typedef void (*voidptr)();

static const voidptr functionalities[] = {
  submit_order,
  print_notes,
  get_note,
  change_note,
  read_note_from_file,
  delete_note,
  add_item,
  change_item,
  delete_item,
  set_fav_item,
  change_fav_item,
  print_fav_item
};

const char *options[] = {
  "Submit Order",

  "List Notes",
  "Add a Note",
  "Change a Note",
  "Read note from file",
  "Delete a Note",

  "Add an Item to your Cart",
  "Change Item Quantity",
  "Delete Item from cart",

  "Set favorite item",
  "Change fav item",
  "Print favorite item",
  NULL
};


/* main */
static int main_loop() {
  print_line('=');

  print_cart(0);

  int input = do_selection("Main Menu", options,
      "Choose an option");

  if (input == 4 && !can_use_admin)
    fputs("Sorry, I can't let you do that (not admin).\n", out);
  else
    functionalities[input]();

  print_line('-');

  return main_loop();
}

int do_server(FILE *min, FILE *mout) {
  in = min;
  out = mout;

  fprintf(out, "Welcome to %s's shopping cart!\n\n", store_name);

  return main_loop();
}

int main(int argc, char **argv) {
  int listener;
  int reuse_addr = 1;
  int local = 0;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--admin")) {
      can_use_admin = 1;
    } else if (!strcmp(argv[i], "--local")) {
      local = 1;
    }
  }
  if (local)
    return do_server(stdin, stdout);

  struct sockaddr_in addr_server = {0};

  if ((listener = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
     fprintf(stderr, "Couldn't open socket\n");
     return 1;
  }

  addr_server.sin_family = AF_INET;
  addr_server.sin_addr.s_addr = htonl(INADDR_ANY);
  addr_server.sin_port = htons(SERVER_PORT);

  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));

  if (bind(listener, (struct sockaddr*)&addr_server, sizeof(addr_server))<0) {
    fprintf(stderr, "Couldn't bind.\n");
    return 1;
  }

  listen(listener, 5);

  while (1) {
    struct sockaddr_in client_addr = {0};
    unsigned int client_addr_size = sizeof(client_addr);
    int client_socket = accept(listener, (struct sockaddr *)&client_addr, (socklen_t*)&client_addr_size);

    int child;

    if (client_socket < 0) {
      fprintf(stderr, "error accepting\n");
      return 1;
    }

    if ( (child = fork()) < 0) {
      fprintf(stderr, "error forking\n");
      return 1;
    } else if (child == 0) {
      close(listener);

      FILE *clientr, *clientw;
      if ((clientr = fdopen(client_socket, "r")) == NULL) {
        fprintf(stderr, "Error opening client fd\n");
        return 1;
      }

      if((clientw = fdopen(client_socket, "w")) == NULL) {
        fprintf(stderr, "Error opening client fd\n");
        return 1;
      }

      do_server(clientr, clientw);
      fflush(clientw);

      fclose(clientw);
      fclose(clientr);

      shutdown(client_socket, SHUT_RDWR);
      close(client_socket);

      return 0;
    } else {
      close(client_socket);
      printf("Accepted connection from %s\n", inet_ntoa(client_addr.sin_addr));
    }
  }

  return 0;
}

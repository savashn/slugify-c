# slugify-c

A C library to convert UTF-8 strings to URL-friendly ASCII slugs.

## Usage

```c
char *slug = slugify("Ecewo Brings Together Web Development & C Programming", NULL);

if (slug) {
    printf("Slug: %s\n", slug);
    free(slug);
} else {
    fprintf(stderr, "Slugify failed\n");
}
```

Output:
```shell
Slug: ecewo-brings-together-web-development-and-c-programming
```

## Options

The second parameter of `slugify()` is for special options.

```c
const char *title = "Всем привет";

slugify_options_t opts = {
    .flags = PRESERVE_CASE,     // Keep the original case (do not lowercase)
    .separator = '_',           // Use underscore instead of dash
    .max_length = 30            // Limit slug to 30 characters
};

char *slug = slugify(title, &opts);

printf("Original: %s\n", title);
printf("Custom slug: %s\n", slug);

free(slug);
```

```shell
Original: Всем привет
Custom slug: Vsem_privet
```

If you want to allow unicode, you can basically add `ALLOW_UNICODE` flag.

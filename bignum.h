#ifndef __BIGNUM__H
#define __BIGNUM__H
#include <linux/slab.h>
#include <linux/string.h>
#define STR_NUM 256
#define XOR_SWAP(a, b, type) \
    do {                     \
        type *__c = (a);     \
        type *__d = (b);     \
        *__c ^= *__d;        \
        *__d ^= *__c;        \
        *__c ^= *__d;        \
    } while (0)


void str_reverse(char *str, size_t n)
{
    for (int i = 0; i < (n >> 1); i++)
        XOR_SWAP(&str[i], &str[n - i - 1], char);
};

static void string_add(char *a, char *b, char *out)
{
    int carry = 0, sum, i = 0;
    size_t a_len = strlen(a), b_len = strlen(b);
    // Check string a is longer than string b
    if (a_len < b_len) {
        XOR_SWAP(a, b, char);
        XOR_SWAP(&a_len, &b_len, size_t);
    }
    for (i = 0; i < b_len; i++) {
        sum = (a[i] - '0') + (b[i] - '0') + carry;
        out[i] = (sum % 10) + '0';
        carry = sum / 10;
    }
    for (i = b_len; i < a_len; i++) {
        sum = (a[i] - '0') + carry;
        out[i] = (sum % 10) + '0';
        carry = sum / 10;
    }

    if (carry)
        out[i++] = carry + '0';
    out[i] = '\0';
};

void is_borrow(char *a, int i, int borrow)
{
    if (borrow) {
        if (a[i] != '0') {
            a[i]--;
        } else {
            int j = i;
            while (a[j] == '0') {
                a[j] = '9';
            }
            a[j + 1]--;
        }
    }
};

void string_sub(char *a, char *b, char *out)
{
    int i = 0, borrow = 0;
    size_t a_len, b_len;
    a_len = strlen(a);
    b_len = strlen(b);
    // String a must be longer than string b

    for (i = 0; i < b_len; i++) {
        is_borrow(a, i, borrow);
        if (a[i] < b[i]) {
            borrow = 1;  // borrow from the next digit
            out[i] = (((a[i] - '0') + 10) - (b[i] - '0')) + '0';

        } else {
            borrow = 0;
            out[i] = ((a[i] - '0') - (b[i] - '0')) + '0';
        }
    }
    for (i = b_len; i < a_len; i++) {
        is_borrow(a, i, borrow);
        out[i] = a[i];
    }
    // Remove the leading 0s e.g 0011 -> 11
    while (out[i - 1] == '0')
        --i;
    out[i] = '\0';
};

void string_mul(char *a, char *b, char *out)
{
    int i = 0;
    size_t a_len = strlen(a);
    size_t b_len = strlen(b);
    size_t total_len = a_len + b_len;
    int count = 0;
    int *value = kmalloc(total_len * sizeof(int), GFP_KERNEL);
    memset(value, 0, sizeof(int) * total_len);
    if (a_len < b_len) {
        XOR_SWAP(a, b, char);
        XOR_SWAP(&a_len, &b_len, size_t);
    }
    // Store the value after multiplication of each digit.
    for (i = 0; i < a_len; i++)
        for (int j = 0; j < b_len; j++) {
            value[i + j] += (a[i] - '0') * (b[j] - '0');
        }
    // Deal with carry
    for (i = 0; i < total_len; i++) {
        value[i + 1] += value[i] / 10;
        value[i] %= 10;
    }
    // Detecting the leading zeros
    while (value[total_len - count - 1] == 0) {
        count++;
    }
    count = total_len - count;
    for (i = 0; i < count; i++)
        out[i] = value[i] + '0';

    out[i] = '\0';
};

#endif

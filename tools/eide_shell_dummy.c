/*
 * EIDE shell build placeholder.
 *
 * The real MSPM0 firmware is built by the root makefile with TI clang.
 * This tiny AC5-compatible translation unit only keeps EIDE's native
 * builder from failing when the project is used as an external-make shell.
 */

typedef void (*EIDE_Vector)(void);

void Reset_Handler(void);

#pragma arm section rodata = "RESET"
const EIDE_Vector __Vectors[] = {
    (EIDE_Vector) 0x20005000,
    Reset_Handler,
};
#pragma arm section

void Reset_Handler(void)
{
    while (1) {
    }
}

int main(void)
{
    while (1) {
    }
}

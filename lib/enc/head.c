/* Assume that *out is large enough to contain the output.
 * Theoretically it should be 4/3 the length of src. */
const unsigned char *c = (const unsigned char *)src;
unsigned char *o = (unsigned char *)out;

/* Use local temporaries to avoid cache thrashing: */
size_t outl = 0;
struct base64_state st;
st.bytes = state->bytes;
st.carry = state->carry;

/* Turn three bytes into four 6-bit numbers: */
/* in[0] = 00111111 */
/* in[1] = 00112222 */
/* in[2] = 00222233 */
/* in[3] = 00333333 */

/* Duff's device, a for() loop inside a switch() statement. Legal! */
switch (st.bytes)
{
	for (;;)
	{
	case 0:

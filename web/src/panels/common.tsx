// Pieces shared by every structure panel.

// What an integer box accepts: an optional minus sign and 1 to 10 digits.
// Only the syntax is checked here; C++ checks the 32-bit range and every rule.
const INTEGER_SYNTAX = /^-?\d{1,10}$/;
export const INTEGER_HINT = 'Enter a whole number: an optional minus sign and 1 to 10 digits.';

/** The number typed in a box, or null when it is not a whole number. */
export function readInteger(text: string): number | null {
  const trimmed = text.trim();
  return INTEGER_SYNTAX.test(trimmed) ? Number(trimmed) : null;
}

export function IntegerField({
  id,
  label,
  value,
  onChange,
  invalid,
  placeholder = 'e.g. 42',
}: {
  id: string;
  label: string;
  value: string;
  onChange: (text: string) => void;
  invalid: boolean;
  placeholder?: string;
}) {
  return (
    <>
      <label htmlFor={id}>{label}</label>
      <input
        id={id}
        type="text"
        inputMode="numeric"
        autoComplete="off"
        placeholder={placeholder}
        value={value}
        aria-invalid={invalid}
        onChange={(event) => onChange(event.target.value)}
      />
    </>
  );
}

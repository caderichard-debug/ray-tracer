import { forwardRef, type ComponentPropsWithoutRef } from "react";

type Variant = "primary" | "ghost" | "outline";

const variants: Record<Variant, string> = {
  primary:
    "bg-accent text-slate-950 hover:bg-accent/90 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent",
  ghost:
    "bg-white/5 text-slate-100 hover:bg-white/10 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-white/40",
  outline:
    "border border-white/15 text-slate-100 hover:border-accent/60 hover:text-accent focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent/60",
};

type ButtonProps = ComponentPropsWithoutRef<"button"> & {
  variant?: Variant;
};

export const Button = forwardRef<HTMLButtonElement, ButtonProps>(
  function Button(
    { className = "", variant = "primary", type = "button", ...props },
    ref,
  ) {
    return (
      <button
        ref={ref}
        type={type}
        className={`inline-flex items-center justify-center gap-2 rounded-full px-5 py-2.5 text-sm font-medium transition duration-200 active:translate-y-px disabled:pointer-events-none disabled:opacity-50 ${variants[variant]} ${className}`}
        {...props}
      />
    );
  },
);
Button.displayName = "Button";

type AnchorButtonProps = ComponentPropsWithoutRef<"a"> & { variant?: Variant };

export function LinkButton({
  className = "",
  variant = "primary",
  ...props
}: AnchorButtonProps) {
  return (
    <a
      className={`inline-flex items-center justify-center gap-2 rounded-full px-5 py-2.5 text-sm font-medium transition duration-200 hover:-translate-y-0.5 ${variants[variant]} ${className}`}
      {...props}
    />
  );
}

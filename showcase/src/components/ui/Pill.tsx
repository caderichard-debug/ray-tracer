type PillProps = {
  children: React.ReactNode;
  active?: boolean;
  onClick?: () => void;
};

export function Pill({ children, active, onClick }: PillProps) {
  const interactive = typeof onClick === "function";

  return (
    <button
      type="button"
      onClick={onClick}
      disabled={!interactive}
      aria-pressed={active}
      className={`rounded-full border px-3 py-1 text-xs font-medium transition ${
        active
          ? "border-accent/60 bg-accent/10 text-accent"
          : "border-white/10 bg-white/5 text-slate-300 hover:border-white/30"
      } ${interactive ? "cursor-pointer" : "cursor-default opacity-80"}`}
    >
      {children}
    </button>
  );
}

export type ProjectCategory = "Rendering" | "Tooling" | "Experience";

export type Project = {
  id: string;
  title: string;
  tagline: string;
  category: ProjectCategory;
  stack: string[];
  problem: string;
  solution: string;
  results: string[];
  imageSrc: string;
  imageAlt: string;
  featured?: boolean;
  status?: "stable" | "wip";
};

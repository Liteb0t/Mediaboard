"use strict";
export const global_styles = new CSSStyleSheet();
const response = await fetch("static/styles.css");
global_styles.replaceSync(await response.text());

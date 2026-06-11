"use strict";

document.addEventListener("DOMContentLoaded", () => {
  const buttons = document.querySelectorAll("[data-logout-button]");
  for (const button of buttons) {
    button.addEventListener("click", async () => {
      button.disabled = true;
      try {
        await fetch("/api/auth/logout", {
          method: "POST",
          headers: { Accept: "application/json" },
        });
      } finally {
        window.location.assign("/login.html");
      }
    });
  }
});

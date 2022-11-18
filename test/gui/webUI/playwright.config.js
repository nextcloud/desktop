// clang-format off
const { devices } = require("@playwright/test");

const config = {
  testDir: "./",
  /* Maximum time one test can run for. */
  timeout: 30 * 1000,
  use: {
    headless: true,
    ignoreHTTPSErrors: true,
  },

  projects: [
    {
      name: "chromium",
      use: {
        ...devices["Desktop Chrome"],
      },
    },
  ],
};

module.exports = config;

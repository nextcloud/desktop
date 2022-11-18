// clang-format off
const { test, expect } = require("@playwright/test");

test("oCIS login", async ({ page }) => {
  const auth_url = process.env.OC_AUTH_URL;
  const username = process.env.OC_USERNAME;
  const password = process.env.OC_PASSWORD;
  if (!auth_url || !username || !password) {
    throw new Error(
      "Some of the following envs are not set:\n" +
        `  OC_AUTH_URL: ${auth_url}\n` +
        `  OC_USERNAME: ${username}\n` +
        `  OC_PASSWORD: ${password}`
    );
  }
  console.info(
    "Login info:\n" +
      `  OC_AUTH_URL: ${auth_url}\n` +
      `  OC_USERNAME: ${username}\n` +
      `  OC_PASSWORD: ${password}`
  );

  await page.goto(auth_url);
  // login
  await page.fill("#oc-login-username", username);
  await page.fill("#oc-login-password", password);
  await page.click("button[type=submit]");
  // allow permissions
  await page.click("button >> text=Allow");
  // confirm successful login
  await page.waitForSelector("text=Login Successful");
});

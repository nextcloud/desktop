// clang-format off
const { test, expect } = require("@playwright/test");

const config = {
  auth_url: "",
  username: "",
  password: "",
};

test.beforeEach(async ({ page }) => {
  config.auth_url = process.env.OC_AUTH_URL;
  config.username = process.env.OC_USERNAME;
  config.password = process.env.OC_PASSWORD;
  if (!config.auth_url || !config.username || !config.password) {
    throw new Error(
      "Some of the following envs are not set:\n" +
        `  OC_AUTH_URL: ${config.auth_url}\n` +
        `  OC_USERNAME: ${config.username}\n` +
        `  OC_PASSWORD: ${config.password}`
    );
  }
  console.info(
    "Login info:\n" +
      `  OC_AUTH_URL: ${config.auth_url}\n` +
      `  OC_USERNAME: ${config.username}\n` +
      `  OC_PASSWORD: ${config.password}`
  );

  await page.goto(config.auth_url);
});

test("oCIS login @oidc", async ({ page }) => {
  // login
  await page.fill("#oc-login-username", config.username);
  await page.fill("#oc-login-password", config.password);
  await page.click("button[type=submit]");
  // allow permissions
  await page.click("button >> text=Allow");
  // confirm successful login
  await page.waitForSelector("text=Login Successful");
});

test("oC10 login @oauth", async ({ page }) => {
  // login
  await page.fill("#user", config.username);
  await page.fill("#password", config.password);
  await page.click("button[type=submit]");
  // authorize
  await page.click("button >> text=Authorize");
  // confirm successful login
  await expect(page.locator("span.error")).toContainText(
    "The application was authorized successfully"
  );
});

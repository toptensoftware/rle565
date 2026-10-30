export default async function ()
{
    // MSVC tool chain
    await this.use("c-cpp");

    // Set variables
    this.set({
        projectKind: "exe",
        define: [ "_CRT_SECURE_NO_WARNINGS" ],
        sourceFiles: (prev) => [ ...prev, "../src/rle565.c" ]
    });
};
function app()
{
    const envBase =
        (typeof window !== "undefined" && window.API_BASE) ||
        (typeof process !== "undefined" && process.env && process.env.API_BASE);
    const apiBase = (envBase || "http://localhost:8080").replace(/\/+$/, "");

    async function showResponse(r)
    {
        const text = r.status === 204 ? "" : await r.text();
        return `HTTP ${r.status}\n${text}`;
    }

    return (
    {
        apiBase,
        lastResponse: "",
        todos: [],
        newTitle: "",

        async ping(path)
        {
            try
            {
                const r = await fetch(`${this.apiBase}${path}`);
                this.lastResponse = await showResponse(r);
            }
            catch (e)
            {
                this.lastResponse = `Fetch error: ${e}`;
            }
        },

        async loadTodos()
        {
            try
            {
                const r = await fetch(`${this.apiBase}/api/todos`,
                {
                    headers:
                    {
                        "Accept": "application/json"
                    }
                });

                const bodyText = r.status === 204 ? "" : await r.text();
                this.lastResponse = `HTTP ${r.status}\n${bodyText}`;
                if (!r.ok) return;

                this.todos = bodyText ? JSON.parse(bodyText) : [];
            }
            catch (e)
            {
                this.lastResponse = `Load error: ${e}`;
            }
        },

        async createTodo()
        {
            const title = this.newTitle.trim();
            if (!title) return;

            try
            {
                const r = await fetch(`${this.apiBase}/api/todos`,
                {
                    method: "POST",
                    headers:
                    {
                        "Content-Type": "application/json",
                        "Accept": "application/json"
                    },
                    body: JSON.stringify({ title })
                });

                this.lastResponse = await showResponse(r);

                if (r.ok)
                {
                    this.newTitle = "";
                    await this.loadTodos();
                }
            }
            catch (e)
            {
                this.lastResponse = `Create error: ${e}`;
            }
        },

        async toggleTodo(t)
        {
            try
            {
                const r = await fetch(`${this.apiBase}/api/todos/${t.id}`,
                {
                    method: "PUT",
                    headers:
                    {
                        "Content-Type": "application/json"
                    },
                    body: JSON.stringify({ title: t.title, done: !t.done })
                });

                this.lastResponse = await showResponse(r);
                if (r.ok) await this.loadTodos();
            }
            catch (e)
            {
                this.lastResponse = `Update error: ${e}`;
            }
        },

        async deleteTodo(t)
        {
            try
            {
                const r = await fetch(`${this.apiBase}/api/todos/${t.id}`,
                {
                    method: "DELETE"
                });

                this.lastResponse = await showResponse(r);
                if (r.ok) await this.loadTodos();
            }
            catch (e)
            {
                this.lastResponse = `Delete error: ${e}`;
            }
        },

        async init()
        {
            await this.loadTodos();
        }
    });
}


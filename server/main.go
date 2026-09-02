package main

import (
    "slowmovie/pkg/api"

    "github.com/labstack/echo/v4"
)

func main() {
    server := api.NewServer()

    e := echo.New()

    api.RegisterHandlers(e, api.NewStrictHandler(
        server,
        // add middlewares here if needed
        []api.StrictMiddlewareFunc{},
    ))

    e.Start("0.0.0.0:8123")
}

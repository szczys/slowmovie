package api

import "context"

type Server struct{}

func NewServer() Server {
	return Server{}
}

func (Server) GetStatus(ctx context.Context, request GetStatusRequestObject) (GetStatusResponseObject, error) {
	return GetStatus200JSONResponse{}, nil
}

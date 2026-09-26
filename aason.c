bool aason_reading(const aason_context* ctx)
{
	rgs_assert(sdd);

	return ctx->reading;
}

bool aason_writing(const aason_context* ctx)
{
	rgs_assert(sdd);

	return !ctx->reading;
}
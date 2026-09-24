bool rgs_sdd_reading(const rgs_sdd* sdd)
{
	rgs_assert(sdd);

	return sdd->reading;
}

bool rgs_sdd_writing(const rgs_sdd* sdd)
{
	rgs_assert(sdd);

	return !sdd->reading;
}
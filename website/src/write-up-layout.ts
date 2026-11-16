import { defineRouteMiddleware } from '@astrojs/starlight/route-data';

export const onRequest = defineRouteMiddleware(({ locals }) => {
  locals.starlightRoute.hasSidebar = false;
});

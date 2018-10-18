using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class card_monitoring_loadDataManager
    {
        //card_monitoring_loadViewModel
        //public long       id          { get; set; }
        //public long?      card_id     { get; set; }
        //public DateTime?  from_date   { get; set; }
        //public DateTime?  to_date     { get; set; }
        //public DateTime?  ts          { get; set; }
        //public int?       status      { get; set; }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="model"></param>
        /// <param name="db"></param>
        public static void Add(card_monitoring_loadViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new card_monitoring_load
            {
                id          = model.id       ,
                card_id     = model.card_id  ,
                from_date   = model.from_date,
                to_date     = model.to_date  ,
                ts          = model.ts       ,
                status      = model.status   ,
            };

            db.card_monitoring_load.Add(dbmodel);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="model"></param>
        /// <param name="db"></param>
        public static void Modify(card_monitoring_loadViewModel model, RAD_PAYEntities db)
        {
            var result = db.card_monitoring_load.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id               ;
                    dbmodel.card_id = model.card_id     ;
                    dbmodel.from_date = model.from_date ;
                    dbmodel.to_date = model.to_date     ;
                    dbmodel.ts = model.ts               ;
                    dbmodel.status = model.status       ;
                }
            }
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="model"></param>
        /// <param name="db"></param>
        public static void Delete(card_monitoring_loadViewModel model, RAD_PAYEntities db)
        {
            var result = db.card_monitoring_load.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.card_monitoring_load.Remove(dbmodel);
                }
            }
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="model"></param>
        /// <param name="db"></param>
        /// <returns></returns>
        public static List<card_monitoring_loadViewModel> Get(card_monitoring_loadViewModel model, RAD_PAYEntities db)
        {
            List<card_monitoring_loadViewModel> list = null;

            var query = from resmodel in db.card_monitoring_load
                        select new card_monitoring_loadViewModel
                        {
                            id = resmodel.id,
                            card_id = resmodel.card_id,
                            from_date = resmodel.from_date,
                            to_date = resmodel.to_date,
                            ts = resmodel.ts,
                            status = resmodel.status,
                        };

            list = query.ToList();

            return list;
        }
    }
}